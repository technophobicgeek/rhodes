/*
 ============================================================================
 Author	    : Dmitry Moskalchuk
 Version	: 1.5
 Copyright  : Copyright (C) 2008 Rhomobile. All rights reserved.

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ============================================================================
 */
package com.rhomobile.rhodes.alert;

import java.io.File;
import java.io.FileInputStream;
import java.util.Map;
import java.util.Vector;

import android.app.Dialog;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.media.MediaPlayer;
import android.net.Uri;
import android.os.Vibrator;
import android.view.Gravity;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup.LayoutParams;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import com.rhomobile.rhodes.AndroidR;
import com.rhomobile.rhodes.Capabilities;
import com.rhomobile.rhodes.Logger;
import com.rhomobile.rhodes.RhodesActivity;
import com.rhomobile.rhodes.RhodesService;
import com.rhomobile.rhodes.file.RhoFileApi;
import com.rhomobile.rhodes.util.PerformOnUiThread;

public class Alert {
	
	private static final String TAG = "Alert";
	
	private static Dialog currentAlert = null;
	private static TextView s_textView = null;
	private static MediaPlayer currentMP = null;
	
	private static native void doCallback(String url, String id, String title);
	
	private static class CustomButton {
		
		public String id;
		public String title;
		
		public CustomButton(String t) {
			id = t;
			title = t;
		}
		
		public CustomButton(String i, String t) {
			id = i;
			title = t;
		}
	};
	
	private static class ShowDialogListener implements OnClickListener {

		private String callback;
		private String id;
		private String title;
		private Dialog dialog;
		
		public ShowDialogListener(String c, String i, String t, Dialog d) {
			callback = c;
			id = i;
			title = t;
			dialog = d;
		}
		
		public void onClick(View arg0) {
			if (callback != null) {
				doCallback(callback, Uri.encode(id), Uri.encode(title));
			}
			dialog.dismiss();
			currentAlert = null;
		}
		
	};

	private static class ShowStatusDialog implements Runnable 
	{
	    private String m_strTitle;
	    private String m_strMessage;
        private String m_strHide;
        
		public ShowStatusDialog(String strTitle, String strMessage, String strHide) 
		{
			m_strMessage = strMessage;
			m_strHide = strHide;
			m_strTitle = strTitle;
		}
		
		public void run() 
		{
			if ( currentAlert != null )
			{
			    s_textView.setText(m_strMessage);
			    return;
			}
			
			Vector<CustomButton> buttons = new Vector<CustomButton>();
			buttons.addElement(new CustomButton(m_strHide));
			
			makeDialog( m_strTitle, m_strMessage, null, buttons, null);
        }
    }
	
	private static class ShowDialog implements Runnable {
		private Object params;
		
		public ShowDialog(Object p) {
			params = p;
		}

		@SuppressWarnings("unchecked")
		public void run() {
			String title = "";//"Alert";
			String message = null;
			Drawable icon = null;
			String callback = null;
			Vector<CustomButton> buttons = new Vector<CustomButton>();
			
			Context ctx = RhodesActivity.getContext();
			
			if (params instanceof String) 
			{
				message = (String)params;
				buttons.addElement(new CustomButton("OK"));
			}
			else if (params instanceof Map<?,?>) {
				Map<Object, Object> hash = (Map<Object, Object>)params;
				
				Object titleObj = hash.get("title");
				if (titleObj != null && (titleObj instanceof String))
					title = (String)titleObj;
				
				Object messageObj = hash.get("message");
				if (messageObj != null && (messageObj instanceof String))
					message = (String)messageObj;
				
				Object iconObj = hash.get("icon");
				if (iconObj != null && (iconObj instanceof String)) {
					String iconName = (String)iconObj;
					if (iconName.equalsIgnoreCase("alert"))
						icon = ctx.getResources().getDrawable(AndroidR.drawable.alert_alert);
					else if (iconName.equalsIgnoreCase("question"))
						icon = ctx.getResources().getDrawable(AndroidR.drawable.alert_question);
					else if (iconName.equalsIgnoreCase("info"))
						icon = ctx.getResources().getDrawable(AndroidR.drawable.alert_info);
					else {
						String iconPath = RhoFileApi.normalizePath("apps/" + iconName);
						Bitmap bitmap = BitmapFactory.decodeStream(RhoFileApi.open(iconPath));
						if (bitmap != null)
							icon = new BitmapDrawable(bitmap);
					}
				}
				
				Object callbackObj = hash.get("callback");
				if (callbackObj != null && (callbackObj instanceof String))
					callback = (String)callbackObj;
				
				Object buttonsObj = hash.get("buttons");
				if (buttonsObj != null && (buttonsObj instanceof Vector<?>)) {
					Vector<Object> btns = (Vector<Object>)buttonsObj;
					for (int i = 0; i < btns.size(); ++i) {
						String itemId = null;
						String itemTitle = null;
						
						Object btnObj = btns.elementAt(i);
						if (btnObj instanceof String) {
							itemId = (String)btnObj;
							itemTitle = (String)btnObj;
						}
						else if (btnObj instanceof Map<?,?>) {
							Map<Object, Object> btnHash = (Map<Object, Object>)btnObj;
							Object btnIdObj = btnHash.get("id");
							if (btnIdObj != null && (btnIdObj instanceof String))
								itemId = (String)btnIdObj;
							Object btnTitleObj = btnHash.get("title");
							if (btnTitleObj != null && (btnTitleObj instanceof String))
								itemTitle = (String)btnTitleObj;
						}
						
						if (itemId == null || itemTitle == null) {
							Logger.E(TAG, "Incomplete button item");
							continue;
						}
						
						buttons.addElement(new CustomButton(itemId, itemTitle));
					}
				}
			}
			
            makeDialog( title, message, icon, buttons, callback);
		}
	};

	private static void makeDialog( String title, String message, Drawable icon, Vector<CustomButton> buttons, String callback)
	{
		if (message == null)
			return;

        Context ctx = RhodesActivity.getInstance();
        int nTopPadding = 10;
        
		Dialog dialog = new Dialog(ctx);
		if ( title == null || title.length() == 0 )
		    dialog.requestWindowFeature(dialog.getWindow().FEATURE_NO_TITLE);
		else    
		{
		    dialog.setTitle(title);
		    nTopPadding = 0;
		}
		    
		dialog.setCancelable(false);
		dialog.setCanceledOnTouchOutside(false);
		
		LinearLayout main = new LinearLayout(ctx);
		main.setOrientation(LinearLayout.VERTICAL);
		main.setLayoutParams(new LayoutParams(LayoutParams.FILL_PARENT, LayoutParams.FILL_PARENT));
		main.setPadding(10, nTopPadding, 10, 10);
		
		LinearLayout top = new LinearLayout(ctx);
		top.setOrientation(LinearLayout.HORIZONTAL);
		top.setGravity(Gravity.CENTER);
		top.setPadding(10, nTopPadding, 10, 10);
		top.setLayoutParams(new LayoutParams(LayoutParams.FILL_PARENT, LayoutParams.WRAP_CONTENT));
		main.addView(top);
		
		if (icon != null) {
			ImageView imgView = new ImageView(ctx);
			imgView.setImageDrawable(icon);
			imgView.setScaleType(ImageView.ScaleType.CENTER);
			imgView.setPadding(10, nTopPadding, 10, 10);
			top.addView(imgView);
		}
		
		TextView textView = new TextView(ctx);
		s_textView = textView;
		textView.setText(message);
		textView.setLayoutParams(new LayoutParams(LayoutParams.FILL_PARENT, LayoutParams.WRAP_CONTENT));
		textView.setGravity(Gravity.CENTER);
		top.addView(textView);
		
		LinearLayout bottom = new LinearLayout(ctx);
		bottom.setOrientation(buttons.size() > 3 ? LinearLayout.VERTICAL : LinearLayout.HORIZONTAL);
		bottom.setGravity(Gravity.CENTER);
		bottom.setLayoutParams(new LayoutParams(LayoutParams.FILL_PARENT, LayoutParams.WRAP_CONTENT));
		main.addView(bottom);
		
		for (int i = 0, lim = buttons.size(); i < lim; ++i) {
			CustomButton btn = buttons.elementAt(i);
			Button button = new Button(ctx);
			button.setText(btn.title);
			button.setOnClickListener(new ShowDialogListener(callback, btn.id, btn.title, dialog));
			button.setLayoutParams(new LinearLayout.LayoutParams(
					lim > 3 ? LayoutParams.FILL_PARENT : LayoutParams.WRAP_CONTENT,
					LayoutParams.WRAP_CONTENT, 1));
			bottom.addView(button);
		}
		
		dialog.setContentView(main);
		dialog.show();
		
		currentAlert = dialog;
	}
	
	private static class HideDialog implements Runnable {
		public void run() {
			if (currentAlert == null)
				return;
			currentAlert.dismiss();
			currentAlert = null;
		}
	};
	
	private static void reportFail(String name, Exception e) {
		Logger.E(TAG, "Call of \"" + name + "\" failed: " + e.getMessage());
	}

	public static void showPopup(Object params) {
		try {
			Logger.T(TAG, "showPopup");
			PerformOnUiThread.exec(new ShowDialog(params), false);
		}
		catch (Exception e) {
			reportFail("showPopup", e);
		}
	}
	
	public static void hidePopup() {
		try {
			Logger.T(TAG, "hidePopup");
			PerformOnUiThread.exec(new HideDialog(), false);
		}
		catch (Exception e) {
			reportFail("hidePopup", e);
		}
	}

	public static void showStatusPopup(String title, String message, String hide) {
		try {
			Logger.I(TAG, "showStatusPopup");
			PerformOnUiThread.exec(new ShowStatusDialog(title, message, hide), false);
		}
		catch (Exception e) {
			reportFail("showStatusPopup", e);
		}
	}
	
	public static void vibrate(int duration) {
		try {
			if (!Capabilities.VIBRATE_ENABLED)
				throw new IllegalAccessException("VIBRATE disabled");
			Logger.T(TAG, "vibrate: " + duration);
			Context ctx = RhodesService.getContext();
			Vibrator vibrator = (Vibrator)ctx.getSystemService(Context.VIBRATOR_SERVICE);
			vibrator.vibrate(duration);
		}
		catch (Exception e) {
			reportFail("vibrate", e);
		}
	}
	
	public static void playFile(String fileName, String mediaType) {
		try {
			Logger.T(TAG, "playFile: " + fileName + " (" + mediaType + ")");
			
			if (currentMP != null)
				currentMP.release();
			
			MediaPlayer mp = new MediaPlayer();
			currentMP = mp;
			mp.setOnErrorListener(new MediaPlayer.OnErrorListener() {
				public boolean onError(MediaPlayer mp, int what, int extra) {
					Logger.E(TAG, "Error when playing file : " + what + ", " + extra);
					return false;
				}
			});
			fileName = RhoFileApi.normalizePath("apps/" + fileName);
			File f = new File(RhodesService.getInstance().getRootPath());
			f = new File(f, fileName);
			if (!f.exists())
				RhoFileApi.copy(fileName);
			
			String source = f.getCanonicalPath();
			Logger.T(TAG, "Final file name: " + source);
			//mp.setDataSource(source);
			
			FileInputStream fs = new FileInputStream(f);
			mp.setDataSource(fs.getFD());
			
			mp.prepare();
			mp.start();
		} catch (Exception e) {
			reportFail("playFile", e);
		}
	}

	public static void stop() {
		try {
			Logger.T(TAG, "stop");
		    	if (currentMP == null) 
			    return;

			if(currentMP.isPlaying()){
			  currentMP.stop();
			}
		}
		catch (Exception e) {
			reportFail("stop", e);
		}
	}
	public static void loop() {
		try {
			Logger.T(TAG, "loop");
		    	if (currentMP == null) 
			    return;

			if(!currentMP.isLooping()){
			  currentMP.setLooping(true);
			}
		}
		catch (Exception e) {
			reportFail("loop", e);
		}
	}

}
