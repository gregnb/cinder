#include "CinderWalletApp.h"
#include "Constants.h"
#include "db/Database.h"
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <QMessageBox>
#include <QStandardPaths>

#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/resource.h>
#include <unistd.h>

#ifdef Q_OS_MAC
#include "util/MacUtils.h"
#endif

// ── Crash log directory (resolved once at startup) ───────────
static char s_crashDir[512] = {};
static char s_logPath[512] = {};

// Async-signal-safe: write a C string to fd
static void writeStr(int fd, const char* s) {
    if (!s) {
        return;
    }
    size_t len = 0;
    while (s[len]) {
        ++len;
    }
    write(fd, s, len);
}

// Async-signal-safe: write an int as decimal to fd
static void writeInt(int fd, int val) {
    char buf[16];
    int i = 0;
    if (val < 0) {
        write(fd, "-", 1);
        val = -val;
    }
    if (val == 0) {
        write(fd, "0", 1);
        return;
    }
    while (val > 0 && i < 15) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    for (int j = i - 1; j >= 0; --j) {
        write(fd, &buf[j], 1);
    }
}

static void crashHandler(int sig) {
    // Build crash file path: <crashDir>/crash_<pid>.log
    char path[600];
    int pi = 0;
    for (int i = 0; s_crashDir[i] && pi < 500; ++i) {
        path[pi++] = s_crashDir[i];
    }
    const char* suffix = "/crash_";
    for (int i = 0; suffix[i]; ++i) {
        path[pi++] = suffix[i];
    }
    // Append PID
    pid_t pid = getpid();
    char pidBuf[16];
    int pidLen = 0;
    pid_t tmp = pid;
    if (tmp == 0) {
        pidBuf[pidLen++] = '0';
    }
    while (tmp > 0 && pidLen < 15) {
        pidBuf[pidLen++] = '0' + (tmp % 10);
        tmp /= 10;
    }
    for (int j = pidLen - 1; j >= 0; --j) {
        path[pi++] = pidBuf[j];
    }
    const char* ext = ".log";
    for (int i = 0; ext[i]; ++i) {
        path[pi++] = ext[i];
    }
    path[pi] = '\0';

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        _exit(127);
    }

    writeStr(fd, "=== Cinder Wallet Crash Report ===\n");
    writeStr(fd, "Signal: ");
    writeInt(fd, sig);
    const char* name = (sig == SIGSEGV)   ? " (SIGSEGV)\n"
                       : (sig == SIGABRT) ? " (SIGABRT)\n"
                       : (sig == SIGBUS)  ? " (SIGBUS)\n"
                                          : " (unknown)\n";
    writeStr(fd, name);
    writeStr(fd, "PID: ");
    writeInt(fd, static_cast<int>(getpid()));
    writeStr(fd, "\n\n--- Stack trace ---\n");

    void* frames[64];
    int count = backtrace(frames, 64);
    backtrace_symbols_fd(frames, count, fd);

    writeStr(fd, "\n--- End ---\n");
    close(fd);

    _exit(127);
}

// Prevent core dumps (key material protection) but capture stack traces.
static void installCrashHandler() {
    // Zero core file size — no memory dumps to disk
    struct rlimit rl = {0, 0};
    setrlimit(RLIMIT_CORE, &rl);

    // Install our crash handler that writes a log before exiting
    struct sigaction sa = {};
    sa.sa_handler = crashHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
    sigaction(SIGSEGV, &sa, nullptr);
}

// ── Qt message handler → log file ────────────────────────────
static FILE* s_logFile = nullptr;

static void messageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg) {
    const char* level = "DEBUG";
    switch (type) {
        case QtDebugMsg:
            level = "DEBUG";
            break;
        case QtInfoMsg:
            level = "INFO ";
            break;
        case QtWarningMsg:
            level = "WARN ";
            break;
        case QtCriticalMsg:
            level = "ERROR";
            break;
        case QtFatalMsg:
            level = "FATAL";
            break;
    }

    QByteArray ts = QDateTime::currentDateTime().toString("hh:mm:ss.zzz").toUtf8();
    QByteArray msgUtf8 = msg.toUtf8();
    const char* file = ctx.file ? ctx.file : "";
    int line = ctx.line;

    if (s_logFile) {
        if (file[0] && line > 0) {
            fprintf(s_logFile, "[%s] %s  %s:%d  %s\n", ts.constData(), level, file, line,
                    msgUtf8.constData());
        } else {
            fprintf(s_logFile, "[%s] %s  %s\n", ts.constData(), level, msgUtf8.constData());
        }
        fflush(s_logFile);
    }

    // Also print to stderr for development
    fprintf(stderr, "[%s] %s  %s\n", ts.constData(), level, msgUtf8.constData());
}

static void initLogging() {
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
    QDir().mkpath(logDir);

    // Store crash dir for the signal handler (must be plain C string)
    QByteArray dirUtf8 = logDir.toUtf8();
    strncpy(s_crashDir, dirUtf8.constData(), sizeof(s_crashDir) - 1);

    // Open rolling log file
    QString logFile = logDir + "/cinder.log";
    QByteArray logPathUtf8 = logFile.toUtf8();
    strncpy(s_logPath, logPathUtf8.constData(), sizeof(s_logPath) - 1);

    // Truncate if over 2 MB to prevent unbounded growth
    QFileInfo fi(logFile);
    if (fi.exists() && fi.size() > 2 * 1024 * 1024) {
        QFile::remove(logFile);
    }

    s_logFile = fopen(s_logPath, "a");
    if (s_logFile) {
        fprintf(s_logFile, "\n========== Session started %s ==========\n",
                QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss").toUtf8().constData());
        fflush(s_logFile);
    }

    qInstallMessageHandler(messageHandler);
}

int main(int argc, char* argv[]) {
    installCrashHandler();

    QApplication app(argc, argv);

    // Set application metadata
    QApplication::setApplicationName("Cinder");
    QApplication::setApplicationVersion(AppVersion::string);
    QApplication::setOrganizationName("Cinder");

    // Start logging to file (needs QStandardPaths, so after QApplication)
    initLogging();

    // Load embedded Exo 2 font (variable weight)
    QFontDatabase::addApplicationFont(":/fonts/Exo2-Variable.ttf");

    // Set Exo 2 as the application-wide default font
    QFont defaultFont("Exo 2", 14);
    app.setFont(defaultFont);

    // Translation is managed by CinderWalletApp for live language switching

    // Initialize database
    if (!Database::open()) {
        qCritical() << "Failed to open or migrate database — aborting startup";
        QMessageBox::critical(nullptr, QObject::tr("Database Error"),
                              QObject::tr("Cinder could not open or migrate your wallet "
                                          "database. The app will now close."));
        return 1;
    }

    app.setWindowIcon(QIcon(":/images/app-icon.png"));

    CinderWalletApp window;
    window.show();

#ifdef Q_OS_MAC
    // Must be called after show() so the native window exists
    setupTransparentTitleBar(window.windowHandle());

    // Set initial tooltips (after toolbar items exist)
    updateSidebarToggleTooltip(QObject::tr("Collapse"));
    updateNotificationBellTooltip(QObject::tr("Notifications"));
#endif

    return app.exec();
}
