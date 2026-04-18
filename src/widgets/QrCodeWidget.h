#ifndef QRCODEWIDGET_H
#define QRCODEWIDGET_H

#include <QVector>
#include <QWidget>

class QrCodeWidget : public QWidget {
    Q_OBJECT

  public:
    explicit QrCodeWidget(QWidget* parent = nullptr);

    void setData(const QString& text);
    void setModuleColor(const QColor& color);
    void setBackgroundColor(const QColor& color);

    QSize sizeHint() const override { return QSize(220, 220); }

  protected:
    void paintEvent(QPaintEvent* event) override;

  private:
    void regenerate();

    QString m_text;
    int m_modules = 0;
    QVector<bool> m_grid; // row-major: m_grid[y * m_modules + x]
    QColor m_moduleColor{0, 0, 0};
    QColor m_bgColor{255, 255, 255};
};

#endif // QRCODEWIDGET_H
