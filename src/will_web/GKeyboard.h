#ifndef GKEYBOARD_H
#define GKEYBOARD_H

#include <QtGui>

#define USE_PROXY_WIDGET

class GWebView;

#ifdef USE_PROXY_WIDGET
class GKeyboard: public QGraphicsProxyWidget {
#else
class GKeyboard: public QGraphicsWidget {
#endif
Q_OBJECT

public:
	GKeyboard(GWebView *webView);

	QLineEdit *inputbox;
	GWebView *webView;
	bool shiftState;

//signals:
//    void characterGenerated(QChar character);

protected:
//	bool event(QEvent *e);
//	void paintEvent(QPaintEvent *e);

private slots:
//	void saveFocusWidget(QWidget *oldFocus, QWidget *newFocus);
	void buttonClicked();
//	void buttonClicked(QWidget *w);

private:
//	QWidget *lastFocusedWidget;
//	QSignalMapper signalMapper;
};

#endif
