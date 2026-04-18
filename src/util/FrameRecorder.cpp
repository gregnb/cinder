#include "FrameRecorder.h"
#include <QDebug>
#include <QPixmap>
#include <QProcess>
#include <QStandardPaths>
#include <QWidget>

FrameRecorder::FrameRecorder(QWidget* target, QObject* parent) : QObject(parent), m_target(target) {
    connect(&m_timer, &QTimer::timeout, this, &FrameRecorder::captureFrame);
    connect(&m_stopTimer, &QTimer::timeout, this, &FrameRecorder::finish);
    m_stopTimer.setSingleShot(true);
}

void FrameRecorder::start(int durationMs, int intervalMs) {
    // Create output directory: ~/Desktop/frame-capture-<timestamp>
    QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QString dirName =
        QString("frame-capture-%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss"));
    m_dir = QDir(desktop + "/" + dirName);
    m_dir.mkpath(".");

    m_frameNum = 0;
    m_elapsed.start();
    m_timer.start(intervalMs);
    m_stopTimer.start(durationMs);

    qDebug() << "[FrameRecorder] Recording to" << m_dir.absolutePath() << "for" << durationMs
             << "ms at" << (1000 / intervalMs) << "fps";
}

void FrameRecorder::captureFrame() {
    if (!m_target || !m_target->isVisible()) {
        return;
    }

    QPixmap frame = m_target->grab();
    qint64 ms = m_elapsed.elapsed();

    // Filename: frame_NNNN_TTTTms.png (sortable, with timestamp for diagnostics)
    QString filename =
        QString("frame_%1_%2ms.png").arg(m_frameNum, 4, 10, QChar('0')).arg(ms, 5, 10, QChar('0'));
    frame.save(m_dir.filePath(filename), "PNG");
    m_frameNum++;
}

void FrameRecorder::stop() {
    m_timer.stop();
    m_stopTimer.stop();
    finish();
}

void FrameRecorder::finish() {
    m_timer.stop();
    m_stopTimer.stop();

    qDebug() << "[FrameRecorder] Captured" << m_frameNum << "frames in" << m_elapsed.elapsed()
             << "ms";
    qDebug() << "[FrameRecorder] Frames saved to" << m_dir.absolutePath();

    // Stitch into video via ffmpeg (non-blocking)
    QString videoPath = m_dir.filePath("startup.mp4");
    QStringList args;
    args << "-framerate" << "30"
         << "-pattern_type" << "glob"
         << "-i" << m_dir.filePath("frame_*.png") << "-c:v" << "libx264"
         << "-pix_fmt" << "yuv420p"
         << "-crf" << "18" << videoPath;

    auto* ffmpeg = new QProcess(this);
    connect(ffmpeg, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [ffmpeg, videoPath](int exitCode, QProcess::ExitStatus) {
                if (exitCode == 0) {
                    qDebug() << "[FrameRecorder] Video saved:" << videoPath;
                } else {
                    qWarning() << "[FrameRecorder] ffmpeg failed:"
                               << ffmpeg->readAllStandardError();
                }
                ffmpeg->deleteLater();
            });

    qDebug() << "[FrameRecorder] Stitching video with ffmpeg...";
    ffmpeg->start("ffmpeg", args);
}
