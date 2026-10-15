package io.deepnotes.app;

import android.net.Uri;
import android.os.Bundle;
import android.webkit.WebView;

import androidx.activity.OnBackPressedCallback;

import com.getcapacitor.BridgeActivity;

public class MainActivity extends BridgeActivity {
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // Back gesture / button: return to the previous page (so a reader who followed a link or opened the playground
        // gets back to the note they were reading); with no history, go to the home page; on the home page, close the app.
        getOnBackPressedDispatcher().addCallback(this, new OnBackPressedCallback(true) {
            @Override
            public void handleOnBackPressed() {
                WebView web = getBridge().getWebView();
                String url = web.getUrl();
                String path = url == null ? null : Uri.parse(url).getPath();
                boolean onHome = path == null || path.isEmpty() || path.equals("/") || path.equals("/index.html");
                if (onHome) {
                    finish();
                } else if (web.canGoBack()) {
                    web.goBack();
                } else {
                    web.loadUrl(getBridge().getLocalUrl() + "/index.html");
                }
            }
        });
    }
}
