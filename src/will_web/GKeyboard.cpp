#include "GKeyboard.h"
#include "GWebView.h"
#include "GProxyLabel.h"

GKeyboard::GKeyboard(GWebView *webView) :
		webView(webView), shiftState(false) {
#ifdef USE_PROXY_WIDGET
	QWidget *container = new QWidget();
	QVBoxLayout *mainLayout = new QVBoxLayout(container);

	QSizePolicy sizePolicy1(QSizePolicy::Ignored, QSizePolicy::Ignored);
	QSizePolicy sizePolicy2(QSizePolicy::Preferred, QSizePolicy::Ignored);
	QSizePolicy sizePolicy3(QSizePolicy::Ignored, QSizePolicy::Ignored);
	sizePolicy1.setHorizontalStretch(5);
	sizePolicy2.setHorizontalStretch(1);

	QFont font;
	font.setPointSize(22);
	font.setBold(true);

	inputbox = new QLineEdit();
	inputbox->setFont(font);
	inputbox->setSizePolicy(sizePolicy1);
	inputbox->setFocusPolicy(Qt::NoFocus);

	GSimpleLabel *submit = new GSimpleLabel("Submit", font, sizePolicy2, true);
	GSimpleLabel *clear = new GSimpleLabel("Clear", font, sizePolicy2, true);
	GSimpleLabel *backspace = new GSimpleLabel("Del<-", font, sizePolicy2,
			true);
	GSimpleLabel *star = new GSimpleLabel("Starred", font, sizePolicy2, true);

	connect(submit, SIGNAL(clicked()), this, SLOT(buttonClicked()));
	connect(clear, SIGNAL(clicked()), this, SLOT(buttonClicked()));
	connect(backspace, SIGNAL(clicked()), this, SLOT(buttonClicked()));
	connect(star, SIGNAL(clicked()), this, SLOT(buttonClicked()));

	QHBoxLayout *layout = new QHBoxLayout();
	layout->addWidget(inputbox);
	layout->addWidget(submit);
	layout->addWidget(clear);
	layout->addWidget(backspace);
	layout->addWidget(star);
	mainLayout->addLayout(layout);

//	QStringList keyboardRows = QString("1 2 3 Q W E R T Y U I O P\n"
//				"4 5 6 A S D F G H J K L\n" "7 8 9 Shift Z X C V B N M , .\n"
//				"0 - + = .com : / @ Space _ ' \" &").split('\n');
	QStringList keyboardRows = QString(
			"( 1 2 3 4 5 6 7 8 9 0 )\n" "- Q W E R T Y U I O P +\n"
					"_ A S D F G H J K L =\n" "Shift * Z X C V B N M , .\n"
					".com : / % & Space @ # ' \" \\").split('\n');
	for (int i = 0; i < keyboardRows.length(); i++) {
		QStringList keys = keyboardRows[i].split(' ');
		layout = new QHBoxLayout();
		for (int i = 0; i < keys.length(); i++) {
			GSimpleLabel *button = new GSimpleLabel(keys[i], font, sizePolicy3,
					true);
			layout->addWidget(button);
			connect(button, SIGNAL(clicked()), this, SLOT(buttonClicked()));
		}
		mainLayout->addLayout(layout);
	}

	container->setLayout(mainLayout);
	setWidget(container);
#else
	QGraphicsLinearLayout *mainLayout = new QGraphicsLinearLayout(Qt::Vertical);
	mainLayout->setSpacing(0);
	mainLayout->setContentsMargins(0, 0, 0, 0);

	QFont font;
	font.setPointSize(18);
	font.setBold(true);

	QSizePolicy sizePolicy1(QSizePolicy::Ignored, QSizePolicy::Ignored);
	QSizePolicy sizePolicy2(QSizePolicy::Minimum, QSizePolicy::Minimum);
	QSizePolicy sizePolicy3(QSizePolicy::Ignored, QSizePolicy::Ignored);
//	sizePolicy1.setHorizontalStretch(5);
//	sizePolicy2.setHorizontalStretch(1);

	inputbox = new QLineEdit();
	inputbox->setFont(font);
	inputbox->setSizePolicy(sizePolicy1);
	inputbox->setFocusPolicy(Qt::NoFocus);

	GLabelProxy *submit = new GLabelProxy("Submit", font, sizePolicy2, true);
	GLabelProxy *clear = new GLabelProxy("Clear", font, sizePolicy2, true);
	GLabelProxy *backspace = new GLabelProxy("Del<-", font, sizePolicy2, true);

	connect(submit->item, SIGNAL(clicked()), this, SLOT(buttonClicked()));
	connect(backspace->item, SIGNAL(clicked()), this, SLOT(buttonClicked()));
	connect(clear->item, SIGNAL(clicked()), this, SLOT(buttonClicked()));

	QGraphicsProxyWidget *inputLineProxy = new QGraphicsProxyWidget();
	inputLineProxy->setWidget(inputbox);

//	inputLineProxy->setSizePolicy(sizePolicy1);
//	submit->setSizePolicy(sizePolicy2);
//	backspace->setSizePolicy(sizePolicy2);
//	clear->setSizePolicy(sizePolicy2);

	QGraphicsLinearLayout *layout = new QGraphicsLinearLayout(Qt::Horizontal);
	layout->setSpacing(0);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addItem(inputLineProxy);
	layout->addItem(submit);
	layout->addItem(clear);
	layout->addItem(backspace);
	mainLayout->addItem(layout);

	QStringList keyboardRows = QString("Q W E R T Y U I O P\n"
			"A S D F G H J K L\n" "Shift Z X C V B N M _\n"
			".com : / - Space \" , . @").split('\n');
	for (int i = 0; i < keyboardRows.length(); i++) {
		QStringList keys = keyboardRows[i].split(' ');
		layout = new QGraphicsLinearLayout(Qt::Horizontal);
		layout->setSpacing(0);
		layout->setContentsMargins(0, 0, 0, 0);
		for (int i = 0; i < keys.length(); i++) {
			GLabelProxy *button = new GLabelProxy(keys[i], font, sizePolicy3,
					true);
//			button->setSizePolicy(sizePolicy3);
			connect(button->item, SIGNAL(clicked()), this,
					SLOT(buttonClicked()));
			layout->addItem(button);
		}
		mainLayout->addItem(layout);
	}

	setLayout(mainLayout);
#endif
}

void GKeyboard::buttonClicked() {
	QLabel *label = (QLabel *) sender();
	QString key = label->text();
	//printf("input key: %s\n", qPrintable(label));

	if (key == "Submit") {
		QString text = inputbox->text();
		if (text == ":q") {
			qApp->quit();
		} else if (text == ":mo") {
			webView->page->mobile = !webView->page->mobile;
		} else if (text == ":img") {
			QWebSettings *settings = QWebSettings::globalSettings();
			bool value = settings->testAttribute(QWebSettings::AutoLoadImages);
			settings->setAttribute(QWebSettings::AutoLoadImages, !value);
//			settings->setAttribute(QWebSettings::PrintElementBackgrounds, !value);
		} else if (text == ":js") {
			QWebSettings *settings = QWebSettings::globalSettings();
			bool value = settings->testAttribute(
					QWebSettings::JavascriptEnabled);
			settings->setAttribute(QWebSettings::JavascriptEnabled, !value);
		} else if (text.startsWith("http://")) {
			inputbox->setText("");
			if (text == "http://g")
				text = "http://www.google.com/?hl=en";
			else if (text == "http://n")
				text = "http://news.google.com/";
			else if (text == "http://w")
				text = "http://www.wikipedia.org/";
			else if (text == "http://y")
				text = "http://www.yahoo.com/";
			else if (text == "http://b")
				text = "http://www.baidu.com/";
			webView->loadUrl(text);
		} else {
			QWebFrame *frame = webView->page->currentFrame();
			// "input:not([type=hidden])"
			QWebElementCollection ee = frame->findAllElements(
					"input[type=text]");
			for (int i = 0; i < ee.count(); i++) {
				QWebElement e = ee[i];
				e.setAttribute("value", text);
			}
			inputbox->setText("");
		}
		hide();
	} else if (key == "Clear") {
		inputbox->setText("");
	} else if (key == "Del<-") {
		QString text = inputbox->text();
		if (text.length() > 0)
			inputbox->setText(text.left(text.length() - 1));
	} else if (key == "Starred") {
		// http://man7.org/linux/man-pages/man3/getline.3.html
		FILE *fp = fopen(".starred", "r");
		if (fp != NULL) {
			char *line = NULL;
			size_t len = 0;
			if (getline(&line, &len, fp) != -1)
				webView->loadUrl(line);
			free(line);
		}
		hide();
	} else if (key == "Shift") {
		shiftState = !shiftState;
	} else if (key == "Space") {
		inputbox->setText(inputbox->text() + " ");
	} else {
		if (!shiftState)
			key = key.toLower();
		inputbox->setText(inputbox->text() + key);
	}
}
