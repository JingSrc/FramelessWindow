#pragma once

#include <QWidget>

namespace Ui {
    class MainWindow;
}

class QSvgRenderer;

class MainWindow : public QWidget
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    virtual ~MainWindow() override;

public:
    void move(int x, int y);
    void move(const QPoint &pos);

protected:
    virtual bool event(QEvent *event) override;
#if (QT_VERSION >= QT_VERSION_CHECK(6,0,0))
    virtual bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#else
    virtual bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;
#endif
    virtual void showEvent(QShowEvent *event) override;
    virtual void resizeEvent(QResizeEvent *event) override;
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseDoubleClickEvent(QMouseEvent *event) override;
    virtual void paintEvent(QPaintEvent *event) override;

private slots:
    void on_titleMinButton_clicked();
    void on_titleMaxButton_clicked();
    void on_titleCloseButton_clicked();

private:
    void generateBackground();

    void changeWindowMaximizedState();

private:
    Ui::MainWindow *ui;

    const int m_padding{ 10 };

    QList<QPixmap> m_background;
};
