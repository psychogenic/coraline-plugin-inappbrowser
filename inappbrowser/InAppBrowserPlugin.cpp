/*
 * InAppBrowserPlugin.cpp
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

#include "InAppBrowserPlugin.h"
#ifdef IAB_THREADSAFISH

#define BROWSERCACHE_LOCKGUARD_OPERATION(opname)         \
        CVDEBUG_OUTLN("Attempting to lock:" << opname);        \
        std::lock_guard<std::mutex> _guard(brw_mutex)

#define BROWSERCACHE_LOCKGUARD_BEGINBLOCK(opname) \
                { \
                BROWSERCACHE_LOCKGUARD_OPERATION(opname)

#define BROWSERCACHE_LOCKGUARD_ENDBLOCK() \
        CVDEBUG_OUTLN("lock block end"); \
                }

#else
#define BROWSERCACHE_LOCKGUARD_OPERATION()
#endif

/*

 struct webview * InAppBrowserPlugin::webviewPtr = NULL;
 GtkWindow * InAppBrowserPlugin::topWin = NULL;
 */
static gboolean mimetypedecision(WebKitWebView *web_view, WebKitWebFrame *frame,
		WebKitNetworkRequest *request, gchar *mimetype,
		WebKitWebPolicyDecision *policy_decision, gpointer user_data) {
	CVDEBUG_OUTLN("mimetypedecision for " << mimetype);
	if (webkit_web_view_can_show_mime_type(web_view, mimetype)) {
		webkit_web_policy_decision_use(policy_decision);
		((InAppBrowserPlugin*) user_data)->shouldShow(web_view);

	} else {
		webkit_web_policy_decision_download(policy_decision);
		((InAppBrowserPlugin*) user_data)->shouldHide(web_view);
	}

	return TRUE;

}
static bool selectFileToSaveAs(GtkWindow* pwin, std::string & saveTo) {
	GtkWidget *dialog;
	GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SAVE;
	gint res;

	dialog = gtk_file_chooser_dialog_new("Save File", pwin, action, "Cancel",
			GTK_RESPONSE_CANCEL, "Save", GTK_RESPONSE_ACCEPT,
			NULL);

	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(pwin));

	if (saveTo.size()) {
		gtk_file_chooser_set_current_name((GtkFileChooser*) dialog,
				saveTo.c_str());
	}

	bool doSave = false;
	res = gtk_dialog_run(GTK_DIALOG(dialog));
	if (res == GTK_RESPONSE_ACCEPT) {
		char *filename;
		GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
		filename = gtk_file_chooser_get_filename(chooser);
		// doit...
		saveTo = filename;
		g_free(filename);
		doSave = true;

	}

	gtk_widget_destroy(dialog);

	return doSave;
}

static gboolean downloadRequested(WebKitWebView* webView,
		WebKitDownload *download, gpointer inappbrowserplugin) {

	InAppBrowserPlugin* plugin = (InAppBrowserPlugin*) inappbrowserplugin;

	CVDEBUG_OUTLN("Download requested... suggested: " << webkit_download_get_suggested_filename(download));
	std::string fname = webkit_download_get_suggested_filename(download);

	if (selectFileToSaveAs(plugin->topWin, fname) && fname.length()) {
		CVDEBUG_OUT("Saving file to : " << fname);

		gchar * tmpFname = g_filename_to_uri(fname.c_str(), NULL, NULL);
		CVDEBUG_OUTLN("...  aka " << tmpFname);

		webkit_download_set_destination_uri(download, tmpFname);
		g_free(tmpFname);

		webkit_download_start(download);  //start the download

	}

	return TRUE;
}

static gboolean decideDestination(WebKitDownload *download,
		gchar *suggested_filename, gpointer inappbrowserplugin) {
	InAppBrowserPlugin* plugin = (InAppBrowserPlugin*) inappbrowserplugin;
	std::string fname;
	if (suggested_filename) {
		fname = suggested_filename;
	}
	gchar* tmpFname;

	if (selectFileToSaveAs(plugin->topWin, fname) && fname.length()) {

		CVDEBUG_OUT("Saving file to : " << fname);
		tmpFname = g_filename_to_uri(fname.c_str(), NULL, NULL);
		CVDEBUG_OUTLN("..." << fname << " aka " << tmpFname);

		webkit_download_set_destination_uri(download, tmpFname);
		g_free(tmpFname);
		return TRUE;
	}

	return FALSE;
}

InAppBrowserPlugin::InAppBrowserPlugin(const Coraline::Plugin::Context & ctx) :
		Coraline::Plugin::Base(ctx), webviewPtr((struct webview*) ctx.view) {

	topWin = GTK_WINDOW(ctx.topWindow);

}

InAppBrowserPlugin::~InAppBrowserPlugin() {

}

void InAppBrowserPlugin::startUp() {
	this->Coraline::Plugin::Base::startUp();
	executeResourceJS("init.js");

	g_signal_connect(G_OBJECT(topWin), "decide-destination",
			G_CALLBACK(decideDestination), this);

}

void InAppBrowserPlugin::registerAllMethods() {

	PLUGINREGMETH(open);
	PLUGINREGMETH(close);
	PLUGINREGMETH(show);
	PLUGINREGMETH(hide);
	PLUGINREGMETH(injectScriptCode);

}

bool InAppBrowserPlugin::hasBrowserWebView(GtkWidget * iapWebView) {

	for (BrowsersCache::iterator iter = browsers.begin();
			iter != browsers.end(); iter++) {
		if (((*iter).second).iapWebView == iapWebView) {

			return true;

		}
	}

	return false;
}
bool InAppBrowserPlugin::hasBrowser(IAPBrowserID id) {

	bool retVal = browsers.find(id) != browsers.end();
	return retVal;
}
InAppBrowserWindows & InAppBrowserPlugin::browserById(IAPBrowserID id) {
	return browsers[id];
}

InAppBrowserWindows & InAppBrowserPlugin::browserByWebView(
		GtkWidget * iapWebView) {

	static InAppBrowserWindows empty;
	for (BrowsersCache::iterator iter = browsers.begin();
			iter != browsers.end(); iter++) {
		if (((*iter).second).iapWebView == iapWebView) {

			return (*iter).second;

		}
	}

	return empty;
}

void InAppBrowserPlugin::containerDestroyed(GtkWidget *wvcontainer) {

	CVDEBUG_OUT("InAppBrowserPlugin::containerDestroyed...");

	BROWSERCACHE_LOCKGUARD_BEGINBLOCK("containerDestroyed");
	for (BrowsersCache::iterator iter = browsers.begin();
			iter != browsers.end(); iter++) {
		if (((*iter).second).contWindow == wvcontainer) {

			CVDEBUG_OUTLN("Found open browser, destroying ref");
			// TODO:FIXME unref??
			browsers.erase(iter);
			return;

		}
	}

	BROWSERCACHE_LOCKGUARD_ENDBLOCK();

	CVDEBUG_OUTLN("no matching container window...?");

}

void InAppBrowserPlugin::shouldShow(WebKitWebView *web_view) {
	BROWSERCACHE_LOCKGUARD_BEGINBLOCK("shouldShow");
	if (hasBrowserWebView(GTK_WIDGET(web_view))) {
		InAppBrowserWindows & bwindows = browserByWebView(GTK_WIDGET(web_view));
		if (bwindows.contWindow) {
			gtk_widget_show_all(bwindows.contWindow);
		}
	}
	BROWSERCACHE_LOCKGUARD_ENDBLOCK();
}

void InAppBrowserPlugin::shouldHide(WebKitWebView *web_view) {
	BROWSERCACHE_LOCKGUARD_BEGINBLOCK("shouldHide");
	if (hasBrowserWebView(GTK_WIDGET(web_view))) {
		InAppBrowserWindows & bwindows = browserByWebView(GTK_WIDGET(web_view));
		if (bwindows.contWindow) {
			gtk_widget_hide(bwindows.contWindow);
		}

	}
	BROWSERCACHE_LOCKGUARD_ENDBLOCK();
}

static void iap_destroy_cb(GtkWidget *widget, gpointer arg) {

	InAppBrowserPlugin * plugin = (InAppBrowserPlugin *) arg;
	plugin->containerDestroyed(widget);

}

InAppBrowserWindows & InAppBrowserPlugin::createNewBrowser(IAPBrowserID id,
		int width, int height, bool useScrollBars) {

	CVDEBUG_OUTLN("Creating new in-app-browser with id: " << id);

	GtkWidget * contWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	GtkWidget * iapWebView = webkit_web_view_new();
	gtk_window_set_default_size(GTK_WINDOW(contWindow), width, height);

	if (useScrollBars) {
		GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_container_add(GTK_CONTAINER(contWindow), scroll);
		gtk_widget_show(scroll);
		gtk_container_add(GTK_CONTAINER(scroll), iapWebView);
	} else {

		gtk_container_add(GTK_CONTAINER(contWindow), iapWebView);
	}

	g_signal_connect(G_OBJECT(iapWebView), "download-requested",
			G_CALLBACK(downloadRequested), this);

	g_signal_connect(G_OBJECT(iapWebView),
			"mime-type-policy-decision-requested",
			G_CALLBACK(mimetypedecision),
			this);

	g_signal_connect(GTK_CONTAINER(contWindow), "destroy",
			G_CALLBACK(iap_destroy_cb), this);

	BROWSERCACHE_LOCKGUARD_BEGINBLOCK("createNewBrowser");
	browsers[id] = InAppBrowserWindows(contWindow, iapWebView);
	BROWSERCACHE_LOCKGUARD_ENDBLOCK();

	return browsers[id];

}

bool InAppBrowserPlugin::launchSystemBrowser(const std::string & uri) {
	GdkScreen *screen = NULL;
	GdkAppLaunchContext *context;
	gboolean ret;
	GdkDisplay *display;

	guint32 timestamp = g_get_real_time();

	if (!uri.size()) {
		return false;
	}

	if (screen != NULL)
		display = gdk_screen_get_display(screen);
	else
		display = gdk_display_get_default();
	context = gdk_display_get_app_launch_context(display);

	gdk_app_launch_context_set_screen(context, screen);
	gdk_app_launch_context_set_timestamp(context, timestamp);

	ret = g_app_info_launch_default_for_uri(uri.c_str(), NULL, NULL);
	g_object_unref(context);
	return ret;
}
bool InAppBrowserPlugin::open(
		const Coraline::Plugin::StandardCallbackIDs & callbacks,
		const Coraline::Plugin::ArgsList & args) {

	PLUGIN_CHECKARGSCOUNT("open", 2, args);
	PLUGIN_CHECKARGTYPE(0, is_number, args); // requested ID
	PLUGIN_CHECKARGTYPE(1, is_string, args); // URI

	int width = IAB_DEFAULT_WIDTH;
	int height = IAB_DEFAULT_HEIGHT;
	json paramsObj;
	std::string target;
	bool useScrollBars = true;
	IAPBrowserID bid = args[0];

	if (hasBrowser(bid)) {
		reportError(callbacks);
		return false;
	}

	std::string uri = args[1];

	if (!uri.size()) {
		reportError(callbacks);
		return false;
	}

	PLUGIN_OPTARG(2, is_string, args, target);
	PLUGIN_OPTARG(3, is_object, args, paramsObj);
	if (paramsObj.size()) {
		if (paramsObj.count("width") && paramsObj["width"].is_number()) {
			width = paramsObj["width"];
		}
		if (paramsObj.count("height") && paramsObj["height"].is_number()) {
			height = paramsObj["height"];
		}
		if (paramsObj.count("scrollbar")
				&& paramsObj["scrollbar"].is_string()) {
			std::string setting = paramsObj["scrollbar"];
			if (setting == "no") {
				useScrollBars = false;
			}
		}

	}

	if (target.size() && target == IAB_TARGET_EXTERNAL_BROWSER) {
		queueAction([this, uri]() {
			launchSystemBrowser(uri);
		});

	} else {
		queueAction(
				[this, bid, uri, width, height, useScrollBars]() {

					InAppBrowserWindows & bwindows = createNewBrowser(bid, width, height, useScrollBars);

					CVDEBUG_OUTLN("Creating web URI request to URI: " << uri);
					gtk_window_set_resizable(GTK_WINDOW(bwindows.contWindow), TRUE);
					gtk_window_set_position(GTK_WINDOW(bwindows.contWindow), GTK_WIN_POS_CENTER);
					webkit_web_view_open(WEBKIT_WEB_VIEW(bwindows.iapWebView), uri.c_str());

				});
	}

	reportSuccess(callbacks);

	return true;
}

bool InAppBrowserPlugin::close(
		const Coraline::Plugin::StandardCallbackIDs & callbacks,
		const Coraline::Plugin::ArgsList & args) {
	PLUGIN_CHECKARGSCOUNT("close", 1, args);
	PLUGIN_CHECKARGTYPE(0, is_number, args);
	IAPBrowserID bid = args[0];

	BROWSERCACHE_LOCKGUARD_BEGINBLOCK("close");
	if (!hasBrowser(bid)) {
		reportError(callbacks);
	}
	InAppBrowserWindows & bwindows = browserById(bid);

	queueAction([bwindows]() {

		gtk_widget_destroy(bwindows.contWindow);
	});

	BROWSERCACHE_LOCKGUARD_ENDBLOCK();

	reportSuccess(callbacks);
	return true;
}

bool InAppBrowserPlugin::show(
		const Coraline::Plugin::StandardCallbackIDs & callbacks,
		const Coraline::Plugin::ArgsList & args) {
	PLUGIN_CHECKARGSCOUNT("show", 1, args);
	PLUGIN_CHECKARGTYPE(0, is_number, args);
	IAPBrowserID bid = args[0];

	BROWSERCACHE_LOCKGUARD_BEGINBLOCK("show");
	if (!hasBrowser(bid)) {
		reportError(callbacks);
	}
	InAppBrowserWindows & bwindows = browserById(args[0]);
	queueAction([bwindows]() {

		gtk_widget_show_all(bwindows.contWindow);
	});
	BROWSERCACHE_LOCKGUARD_ENDBLOCK();

	reportSuccess(callbacks);

	return true;

}

bool InAppBrowserPlugin::hide(
		const Coraline::Plugin::StandardCallbackIDs & callbacks,
		const Coraline::Plugin::ArgsList & args) {
	PLUGIN_CHECKARGSCOUNT("hide", 1, args);
	PLUGIN_CHECKARGTYPE(0, is_number, args);
	IAPBrowserID bid = args[0];
	BROWSERCACHE_LOCKGUARD_BEGINBLOCK("hide");
	if (!hasBrowser(bid)) {
		reportError(callbacks);
	}
	InAppBrowserWindows & bwindows = browserById(args[0]);
	queueAction([bwindows]() {

		gtk_widget_hide(bwindows.contWindow);
	});

	BROWSERCACHE_LOCKGUARD_ENDBLOCK();
	return true;

}

bool InAppBrowserPlugin::injectScriptCode(
		const Coraline::Plugin::StandardCallbackIDs & callbacks,
		const Coraline::Plugin::ArgsList & args) {
	PLUGIN_CHECKARGSCOUNT("injectScriptCode", 2, args);
	PLUGIN_CHECKARGTYPE(0, is_number, args);
	PLUGIN_CHECKARGTYPE(1, is_string, args);
	if (!hasBrowser(args[0])) {
		reportError(callbacks);
	}
	InAppBrowserWindows & bwindows = browserById(args[0]);

	std::string theJS = args[1];
	webkit_web_view_execute_script(WEBKIT_WEB_VIEW(bwindows.iapWebView),
			theJS.c_str());
	reportSuccess(callbacks);

	return true;

}
