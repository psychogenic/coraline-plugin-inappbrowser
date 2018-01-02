/*
 * InAppBrowserPlugin.h
 *
 *  Created on: Dec 28, 2017
 *      
 *        Copyright (C) 2017 Pat Deegan, https://psychogenic.com
 *
 *  This file is part of the Coraline In-App Browser plugin [https://coraline.psychogenic.com/]
 *
 *  Coraline is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Coraline is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Coraline.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef INAPPBROWSER_INAPPBROWSERPLUGIN_H_
#define INAPPBROWSER_INAPPBROWSERPLUGIN_H_

#include "inappbrowserExtIncludes.h"

#define MYPLUGIN_VERSION_MAJOR			1
#define MYPLUGIN_VERSION_MINOR 			0
#define MYPLUGIN_VERSION_PATCH 			2


#define IAB_TARGET_EXTERNAL_BROWSER		"external"

#define IAB_DEFAULT_WIDTH		300
#define IAB_DEFAULT_HEIGHT		300
#define IAB_THREADSAFISH


typedef struct InAppBrowserWindowsStruct {

    GtkWidget * contWindow;
    GtkWidget * iapWebView;
    InAppBrowserWindowsStruct() : contWindow(NULL), iapWebView(NULL) {}
    InAppBrowserWindowsStruct(GtkWidget* container, GtkWidget* webview) : contWindow(container), iapWebView(webview) {}

}InAppBrowserWindows ;

typedef int IAPBrowserID;

typedef std::map<IAPBrowserID, InAppBrowserWindows>	BrowsersCache;

class InAppBrowserPlugin : public Coraline::Plugin::Base  {
public:

	InAppBrowserPlugin(const Coraline::Plugin::Context & ctx);
	virtual ~InAppBrowserPlugin();


	virtual void startUp();

	Coraline::Version version() { return Coraline::Version();}

    virtual const std::string clobbers() { return "window.open";}
    virtual const Coraline::PluginName fullName() { return "cordova-plugin-inappbrowser"; }
    virtual const Coraline::PluginName shortName() { return "InAppBrowser";}



	struct webview * webviewPtr;
	GtkWindow * topWin;


	/* used internally */
	void shouldShow(WebKitWebView *web_view);
	void shouldHide(WebKitWebView *web_view);
	void containerDestroyed(GtkWidget *wvcontainer);

protected:
	virtual void registerAllMethods();

private:

	bool launchSystemBrowser(const std::string & uri);



	InAppBrowserWindows & createNewBrowser(IAPBrowserID id,
			int width=IAB_DEFAULT_WIDTH, int height=IAB_DEFAULT_HEIGHT,
			bool useScrollBars=true);

	bool hasBrowser(IAPBrowserID id);
	bool hasBrowserWebView(GtkWidget * iapWebView);

	InAppBrowserWindows & browserById(IAPBrowserID id);
	InAppBrowserWindows & browserByWebView(GtkWidget * iapWebView);

	bool open(const Coraline::Plugin::StandardCallbackIDs & callbacks,
								const Coraline::Plugin::ArgsList & args);
	bool close(const Coraline::Plugin::StandardCallbackIDs & callbacks,
								const Coraline::Plugin::ArgsList & args);
	bool show(const Coraline::Plugin::StandardCallbackIDs & callbacks,
								const Coraline::Plugin::ArgsList & args);
	bool hide(const Coraline::Plugin::StandardCallbackIDs & callbacks,
								const Coraline::Plugin::ArgsList & args);

	bool injectScriptCode(const Coraline::Plugin::StandardCallbackIDs & callbacks,
								const Coraline::Plugin::ArgsList & args);


	BrowsersCache browsers;

#ifdef IAB_THREADSAFISH
        std::mutex brw_mutex;
#endif



};

#endif /* INAPPBROWSER_INAPPBROWSERPLUGIN_H_ */
