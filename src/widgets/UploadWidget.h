#ifndef UPLOADWIDGET_H
#define UPLOADWIDGET_H

#include <QPixmap>
#include <QString>
#include <QWidget>

class UploadWidget : public QWidget {
    Q_OBJECT
  public:
    enum Shape { Circle, RoundedRect };

    explicit UploadWidget(Shape shape, int size, QWidget* parent = nullptr);

    void setPlaceholderText(const QString& text);
    void setFileFilter(const QString& filter);
    void setMaxResolution(int px);

    QString imagePath() const;
    QPixmap pixmap() const;
    bool hasImage() const;

    void setPixmap(const QPixmap& pm);
    void setImagePath(const QString& path);
    void clear();

  signals:
    void imageSelected(const QString& path);
    void imageCleared();

  protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

  private:
    Shape m_shape;
    int m_size;
    QString m_placeholder = "+";
    QString m_filter = "Images (*.png *.jpg *.jpeg *.webp)";
    int m_maxResolution = 256;

    QPixmap m_pixmap;
    QString m_imagePath;
    bool m_hovered = false;
};

#endif // UPLOADWIDGET_H
