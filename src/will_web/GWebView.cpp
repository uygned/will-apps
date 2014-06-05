#include "GWebView.h"

GWebView::GWebView() :
		QGraphicsWebView() {
	loaded = false;

	page = new GWebPage();
	page->mobile = true;
	setPage(page);

	page->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);
	connect(page, SIGNAL(linkClicked(const QUrl &)), this,
			SLOT(onLinkClicked(const QUrl &)));
	connect(page, SIGNAL(loadFinished (bool)), this,
			SLOT(onLoadFinished(bool)));
//	connect(this, SIGNAL(urlChanged(const QUrl &)), this,
//			SLOT(onUrlChanged(const QUrl &)));
	connect(page, SIGNAL(unsupportedContent(QNetworkReply *)), this,
			SLOT(onUnsupportedContent(QNetworkReply *)));

//	connect(page, SIGNAL(downloadRequested(const QNetworkRequest &)), this,
//			SLOT(onDownloadRequested(const QNetworkRequest &)));

//	connect(page->networkAccessManager(),
//			SIGNAL(sslErrors(QNetworkReply *, const QList<QSslError> &)), this,
//			SLOT(onSslErrors(QNetworkReply *, const QList<QSslError> &)));

//setResizesToContents(true);

//	page()->mainFrame()->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAlwaysOff);
//	page()->mainFrame()->setScrollBarPolicy(Qt::Vertical, Qt::ScrollBarAlwaysOff);

	connect(&downloader, SIGNAL(finished(QNetworkReply *)), this,
			SLOT(onDownloadFinished(QNetworkReply *)));

	toolbar = new GToolbar(this);
	keyboard = new GKeyboard(this);
	inputContext = new GInputContext(keyboard);
	qApp->setInputContext(inputContext);

	QSizePolicy sizePolicy = QSizePolicy(QSizePolicy::Maximum,
			QSizePolicy::Minimum);
	infobar = new GProxyLabel("", font(), sizePolicy, true);
	infobar->item->setAlignment(Qt::AlignLeft);
	infobar->hide();

	QWebSettings *settings = QWebSettings::globalSettings();
	settings->setMaximumPagesInCache(3);
	settings->setFontSize(QWebSettings::DefaultFontSize, 20);
	settings->setAttribute(QWebSettings::DeveloperExtrasEnabled, false);
	settings->setAttribute(QWebSettings::JavascriptCanOpenWindows, false);
	settings->setAttribute(QWebSettings::JavascriptCanCloseWindows, false);

//	settings->setFontFamily(QWebSettings::StandardFont, "XinGothic-CiticPress");
//	settings->setFontFamily(QWebSettings::FixedFont, "XinGothic-CiticPress");
//	settings->setFontFamily(QWebSettings::SerifFont, "XinGothic-CiticPress");
//	settings->setFontFamily(QWebSettings::SansSerifFont,
//			"XinGothic-CiticPress");

//	settings->setAttribute(QWebSettings::TiledBackingStoreEnabled, true);
//	settings->setAttribute(QWebSettings::FrameFlatteningEnabled, true);
}

GWebView::~GWebView() {
	// owned by QGraphicsScene
//	delete toolbar;
//	delete keyboard;
//	delete inputContext;
}

void GWebView::loadUrl(QString url) {
	//	if (url.contains("www.google.com")) {
	//		int i = url.indexOf("url?q=");
	//		QUrl::fromPercentEncoding();
	//	}
	infobar->item->setText(QString("Loading %1...").arg(url));
	infobar->show();
	loaded = false;
	load(url);
}

void GWebView::onLinkClicked(const QUrl &url) {
	QString urlStr = url.toString();
//	printf("linkClicked: %s\n", qPrintable(urlStr));
	if (page->mobile && urlStr.indexOf("/url?") != -1
			&& (urlStr.startsWith("http://www.google.com")
					|| urlStr.startsWith("https://www.google.com"))) {
		// we have 2 types of URLs:
		// 1. http://www.google.com.hk/url?q=<url>
		// 2. http://www.google.com.hk/search?q=<query>
		QByteArray q = url.encodedQueryItemValue("q");
		if (q.isEmpty())
			q = url.encodedQueryItemValue("url");
		if (!q.isEmpty())
			urlStr = QUrl::fromPercentEncoding(q);
	}
	if (urlStr.endsWith(".pdf")) {
		infobar->item->setText(QString("Downloading %1...").arg(urlStr));
		infobar->show();
		downloader.get(QNetworkRequest(QUrl(urlStr)));
	} else {
		loadUrl(urlStr);
	}
}

//void GWebView::onUrlChanged(const QUrl &url) {
//	QString urlStr = url.toString();
//	if (urlStr.endsWith(".pdf")) {
//		infobar->item->setText(QString("Downloading redirected %1...").arg(urlStr));
//		infobar->show();
//		downloader.get(QNetworkRequest(url));
//	}
//}

void GWebView::onLoadFinished(bool ok) {
	if (!ok)
		return;

//	QWebElementCollection eles = page->mainFrame()->findAllElements("img");
//	for (int i = 0; i < eles.count(); i++) {
//		QString src = eles[i].attribute("src");
//		if (src.endsWith(".gif"))
//			eles[i].setAttribute("src", "");
//	}

	infobar->hide();
	loaded = true;

//	QWebElement body = page()->mainFrame()->findFirstElement("body");
//	page()->mainFrame()->evaluateJavaScript(
//			"document.getElementsByTagName('body')[0].style.webkitTransform = \"rotate(90deg)\";");
//	page()->mainFrame()->evaluateJavaScript("document.getElementsByTagName('body')[0].style.setProperty(\"-webkit-transform\", \"rotate(90deg)\", null);");
}

void GWebView::onUnsupportedContent(QNetworkReply *reply) {
	QString urlStr = reply->url().toString();
	if (urlStr.endsWith(".pdf")) {
		onDownloadFinished(reply);
	} else {
		infobar->item->setText(QString("Unsupported: %1").arg(urlStr));
		infobar->item->show();
	}
}

//void GWebView::onDownloadRequested(const QNetworkRequest &request) {
//	infobar->item->setText(
//			QString("Downloading %1...").arg(request.url().toString()));
//	infobar->show();
//	downloader.get(request);
//}

void GWebView::onDownloadFinished(QNetworkReply *reply) {
	if (reply->error()) {
		infobar->item->setText(
				QString("Download failed: %1").arg(reply->errorString()));
	} else {
		QString dir = "/mnt/us/downloads";
		QString date = QDateTime::currentDateTime().toString("yyyy-MM-dd");
		QDir d(dir);
		if (!d.exists(date) && !d.mkdir(date)) {
			infobar->item->setText(
					QString("Can't make directory: %1").arg(dir + "/" + date));
		} else {
			QUrl url = reply->url();
			QString fileName = QFileInfo(url.path()).fileName();
			if (fileName.isEmpty())
				fileName = "download";

			fileName = dir + "/" + date + "/" + fileName;
			if (QFile::exists(fileName)) {
				int i = 0;
				fileName += '.';
				while (QFile::exists(fileName + QString::number(i)))
					++i;
				fileName += QString::number(i);
			}

			QFile file(fileName);
			if (!file.open(QIODevice::WriteOnly)) {
				infobar->item->setText(
						QString("Can't open file: %1 (%2)").arg(fileName).arg(
								file.errorString()));
			} else {
				file.write(reply->readAll());
				file.close();
				infobar->item->setText(
						QString("Downloaded: %1").arg(url.toString()));
				if (fileName.endsWith(".pdf")) {
					char cmd[256];
					strcpy(cmd, "/mnt/us/extensions/willapps/willpdf.exec ");
					strcat(cmd, fileName.toUtf8().constData());
					system(cmd);
				}
			}
		}
	}
	reply->deleteLater();
//	infobar->hide();
}

//void GWebView::onSslErrors(QNetworkReply *reply,
//		const QList<QSslError> &errors) {
//	if (!errors.isEmpty()) {
//		infobar->item->setText(
//				QString("SSL error: %1").arg(errors[0].errorString()));
//		infobar->item->show();
//	}
//	reply->ignoreSslErrors();
//}

QString GWebPage::userAgentForUrl(const QUrl& url) const {
	Q_UNUSED(url);
	// Mozilla/5.0 (%Platform%%Security%%Subplatform%) AppleWebKit/%WebKitVersion% (KHTML, like Gecko) %AppVersion Safari/%WebKitVersion%
	// Mozilla/5.0 (Unknown; Linux) AppleWebKit/534.34 (KHTML, like Gecko) Qt/4.8.5 Safari/534.34
	return QString(
			"Mozilla/5.0 (Linux) AppleWebkit/%1 (KHTML, like Gecko) Version/5.0 %2Safari/%3").arg(
			qWebKitVersion()).arg(mobile ? "Mobile " : "").arg(qWebKitVersion());
}
