#include "GWebView.h"
#include "GMultiTouch.h"

GWebView *webView;
QSize screenSize(758, 1024);
int screenRotation = 0;
int toolbarHeight = 80;
int keyboardHeight = 350;

void resize(int rotation) {
	screenRotation = rotation;

	if (rotation != 0) {
		webView->rotate(rotation);
		webView->toolbar->rotate(rotation);
		webView->keyboard->rotate(rotation);
		webView->infobar->rotate(rotation);

		if (rotation == 270) {
			toolbarHeight = 65;
			webView->toolbar->resize(screenSize.height(), toolbarHeight);
			webView->toolbar->setPos(0, 0);

			webView->resize(screenSize.height(),
					screenSize.width() - toolbarHeight);
			webView->setPos(toolbarHeight, 0);

			webView->keyboard->resize(screenSize.height(), keyboardHeight);
			webView->keyboard->setPos(screenSize.width() - keyboardHeight, 0);

			int infobarHeight = webView->infobar->size().height();
			webView->infobar->resize(screenSize.height(), infobarHeight);
			webView->infobar->setPos(screenSize.width() - infobarHeight, 0);
		}

		return;
	}

	toolbarHeight = 70;

	webView->toolbar->resize(screenSize.width(), toolbarHeight);
	webView->toolbar->setPos(0, 0);

	webView->resize(screenSize.width(), screenSize.height() - toolbarHeight);
	webView->setPos(0, toolbarHeight);

	webView->keyboard->resize(screenSize.width(), keyboardHeight);
	webView->keyboard->setPos(0, screenSize.height() - keyboardHeight);

	int infobarHeight = webView->infobar->size().height();
	webView->infobar->resize(screenSize.width(), infobarHeight);
	webView->infobar->setPos(screenSize.height() - infobarHeight, 0);
}

// http://stackoverflow.com/questions/14616342/example-code-for-a-simple-web-page-browser-using-webkit-qt-in-c
int main(int argc, char **argv) {
	QApplication app(argc, argv);

	webView = new GWebView();

	QGraphicsScene scene;
	QGraphicsView view(&scene);
	view.setFrameShape(QFrame::NoFrame);
	view.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	view.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	scene.addItem(webView);
	scene.addItem(webView->toolbar);
	scene.addItem(webView->keyboard);
	scene.addItem(webView->infobar);
	webView->keyboard->hide();

	if (argc > 1 && strcmp(argv[1], "-d") == 0)
		resize(0);
	else
		resize(270);

//	view.resize(screenSize.width(), screenSize.height() - toolbarHeight);
//	view.move(0, toolbarHeight);
	view.resize(screenSize.width(), screenSize.height());
	view.move(0, 0);
	view.show();

#if defined(Q_WS_QPA)
	GMultiTouch multiTouch;
#endif

	if (argc > 1) {
		printf("Loading %s %d (%fx%f)\n", argv[1], argc,
				webView->size().width(), webView->size().height());
//		webView->load(QUrl(argv[1]));
		webView->loadUrl(argv[1]);
	} else {
		webView->setHtml("<html><body>hello world!</body></html>", QUrl(""));
	}

	return app.exec();
}
