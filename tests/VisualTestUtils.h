#ifndef VISUALTESTUTILS_H
#define VISUALTESTUTILS_H

#include <gtest/gtest.h>

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QTest>
#include <QVBoxLayout>
#include <QWidget>

namespace VisualTestUtils {

    inline QString repoOwnedVisualRoot(const QString& suiteName) {
        const QString testsDir = QFileInfo(QString::fromUtf8(__FILE__)).absolutePath();
        return QDir(testsDir).filePath(QStringLiteral("visual-baselines/%1").arg(suiteName));
    }

    inline QImage normalizeCapturedImage(const QImage& image) {
        const qreal dpr = image.devicePixelRatio();
        QImage normalized = image;
        if (dpr > 1.0) {
            const QSize logicalSize = image.deviceIndependentSize().toSize();
            normalized = image.scaled(logicalSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
        return normalized.convertToFormat(QImage::Format_RGB32);
    }

    inline void settleUi(int ms = 40) {
        QApplication::processEvents();
        QTest::qWait(ms);
        QApplication::processEvents();
    }

    class VisualRecorder {
      public:
        VisualRecorder(const QString& defaultRoot, const QString& rootVarName,
                       const QString& updateVarName, const QString& maxDiffVarName) {
            m_rootDir = qEnvironmentVariable(rootVarName.toUtf8().constData(), defaultRoot);
            m_baselineDir = m_rootDir + "/baseline";
            m_currentDir = m_rootDir + "/current";
            m_diffDir = m_rootDir + "/diff";
            m_updateBaseline =
                qEnvironmentVariableIntValue(updateVarName.toUtf8().constData()) == 1;
            m_maxDiffPixels = qEnvironmentVariableIntValue(maxDiffVarName.toUtf8().constData());

            QDir().mkpath(m_baselineDir);
            QDir().mkpath(m_currentDir);
            QDir().mkpath(m_diffDir);
        }

        void record(const QString& frameName, const QImage& image, int maxDiffPixels = -1) {
            const QImage normalizedImage = image.convertToFormat(QImage::Format_RGB32);
            const QString safeName = frameName + ".png";
            const QString baselinePath = m_baselineDir + "/" + safeName;
            const QString currentPath = m_currentDir + "/" + safeName;
            const QString diffPath = m_diffDir + "/" + safeName;
            normalizedImage.save(currentPath);

            const int maxDiff = maxDiffPixels >= 0 ? maxDiffPixels : m_maxDiffPixels;

            if (m_updateBaseline || !QFile::exists(baselinePath)) {
                normalizedImage.save(baselinePath);
                QFile::remove(diffPath);
                return;
            }

            QImage baseline(baselinePath);
            ASSERT_FALSE(baseline.isNull())
                << "Missing baseline frame: " << baselinePath.toStdString();
            baseline = baseline.convertToFormat(QImage::Format_RGB32);

            ASSERT_EQ(baseline.size(), normalizedImage.size())
                << "Frame size mismatch for " << frameName.toStdString()
                << " baseline=" << baseline.width() << "x" << baseline.height()
                << " current=" << normalizedImage.width() << "x" << normalizedImage.height();

            int diffPixels = 0;
            QImage diff(normalizedImage.size(), QImage::Format_ARGB32_Premultiplied);
            diff.fill(Qt::transparent);

            for (int y = 0; y < normalizedImage.height(); ++y) {
                const QRgb* a = reinterpret_cast<const QRgb*>(baseline.constScanLine(y));
                const QRgb* b = reinterpret_cast<const QRgb*>(normalizedImage.constScanLine(y));
                QRgb* d = reinterpret_cast<QRgb*>(diff.scanLine(y));
                for (int x = 0; x < normalizedImage.width(); ++x) {
                    if (a[x] != b[x]) {
                        ++diffPixels;
                        d[x] = qRgba(255, 0, 255, 220);
                    } else {
                        d[x] = qRgba(0, 0, 0, 0);
                    }
                }
            }

            if (diffPixels > maxDiff) {
                QImage overlay =
                    normalizedImage.convertToFormat(QImage::Format_ARGB32_Premultiplied);
                QPainter painter(&overlay);
                painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
                painter.drawImage(0, 0, diff);
                painter.end();
                overlay.save(diffPath);
            } else {
                QFile::remove(diffPath);
            }

            EXPECT_LE(diffPixels, maxDiff)
                << "Visual regression in frame " << frameName.toStdString()
                << " diffPixels=" << diffPixels << " max=" << maxDiff
                << " current=" << currentPath.toStdString()
                << " baseline=" << baselinePath.toStdString() << " diff=" << diffPath.toStdString();
        }

        QString rootDir() const { return m_rootDir; }

      private:
        QString m_rootDir;
        QString m_baselineDir;
        QString m_currentDir;
        QString m_diffDir;
        bool m_updateBaseline = false;
        int m_maxDiffPixels = 0;
    };

    inline void capturePage(QWidget& widget, VisualRecorder& recorder, const QString& frameName,
                            int waitMs = 80, int maxDiffPixels = -1, int captureWidth = 1440,
                            int captureHeight = 900,
                            const QString& hostObjectName = "visualCaptureHost",
                            const QColor& background = QColor(18, 19, 31)) {
        QWidget host;
        host.setObjectName(hostObjectName);
        host.setWindowFlag(Qt::FramelessWindowHint, true);
        host.resize(captureWidth, captureHeight);

        auto* hostLayout = new QVBoxLayout(&host);
        hostLayout->setContentsMargins(0, 0, 0, 0);
        hostLayout->setSpacing(0);
        hostLayout->addWidget(&widget);

        widget.resize(captureWidth, captureHeight);
        host.show();
        settleUi(waitMs);

        if (host.size() != QSize(captureWidth, captureHeight)) {
            host.resize(captureWidth, captureHeight);
            settleUi(10);
        }

        (void)host.grab();
        settleUi(30);

        QPixmap captured = host.grab();
        const qreal dpr = captured.devicePixelRatio();
        QImage image = captured.toImage().convertToFormat(QImage::Format_RGB32);
        if (dpr > 1.0) {
            const int logicalW = qRound(captured.width() / dpr);
            const int logicalH = qRound(captured.height() / dpr);
            image =
                image.scaled(logicalW, logicalH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }

        if (image.size() != QSize(captureWidth, captureHeight)) {
            QImage normalized(captureWidth, captureHeight, QImage::Format_RGB32);
            normalized.fill(background);
            QPainter painter(&normalized);
            painter.drawImage(0, 0, image);
            painter.end();
            image = normalized;
        }

        recorder.record(frameName, image, maxDiffPixels);
        hostLayout->removeWidget(&widget);
        widget.hide();
        widget.setParent(nullptr);
        host.hide();
        settleUi(20);
    }

} // namespace VisualTestUtils

#endif // VISUALTESTUTILS_H
