#ifndef GWEBVIEW_H
#define GWEBVIEW_H

#include <QtWebKit/QtWebKit>
#include <QtWebKit/QWebView>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>

#include "GToolbar.h"
#include "GKeyboard.h"
#include "GInputContext.h"
#include "GProxyLabel.h"

class GWebPage: public QWebPage {
Q_OBJECT
public:
	bool mobile;
protected:
	QString userAgentForUrl(const QUrl& url) const;
};

class GWebView: public QGraphicsWebView {
Q_OBJECT

public:
	GWebView();
	~GWebView();

	bool loaded;
	GWebPage *page;
	QNetworkAccessManager downloader;
	GToolbar *toolbar;
	GInputContext *inputContext;
	GKeyboard *keyboard;
	GProxyLabel *infobar;

	void loadUrl(QString url);

private slots:
	void onLinkClicked(const QUrl &url);
	void onLoadFinished(bool ok);
//	void onUrlChanged(const QUrl &url);
	void onDownloadFinished(QNetworkReply *reply);
//	void onDownloadRequested(const QNetworkRequest &request);
	void onUnsupportedContent(QNetworkReply *reply);

//	void onSslErrors(QNetworkReply *reply, const QList<QSslError> &errors);
};

#endif
