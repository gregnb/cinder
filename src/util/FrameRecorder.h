#ifndef FRAMERECORDER_H
#define FRAMERECORDER_H

#include <QDir>
#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

class QWidget;

// Captures a target widget at ~30fps to numbered PNGs, then optionally
// stitches them into a video via ffmpeg.  Intended for diagnosing startup
// rendering delays (chart/activity card pop-in, etc.).
//
// Usage:
//   auto* rec = new FrameRecorder(mainWindow, mainWindow);
//   rec->start();          // begins capturing
//   // ... after N seconds it auto-stops, or call rec->stop() manually
//
class FrameRecorder : public QObject {
    Q_OBJECT

  public:
    explicit FrameRecorder(QWidget* target, QObject* parent = nullptr);

    void start(int durationMs = 8000, int intervalMs = 33);
    void stop();

    QString outputDir() const { return m_dir.absolutePath(); }

  private slots:
    void captureFrame();
    void finish();

  private:
    QWidget* m_target;
    QTimer m_timer;
    QTimer m_stopTimer;
    QElapsedTimer m_elapsed;
    QDir m_dir;
    int m_frameNum = 0;
};

#endif // FRAMERECORDER_H
