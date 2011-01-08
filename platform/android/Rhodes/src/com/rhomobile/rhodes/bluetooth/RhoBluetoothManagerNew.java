package com.rhomobile.rhodes.bluetooth;


import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.content.Intent;
import android.content.pm.FeatureInfo;
import android.content.pm.PackageManager;
import android.util.Log;
import com.rhomobile.rhodes.RhodesActivity;
import com.rhomobile.rhodes.RhodesService;
import com.rhomobile.rhodes.util.PerformOnUiThread;

public class RhoBluetoothManagerNew implements IRhoBluetoothManager {



	private boolean mBluetoothIsEnabled = false;
	
    private static final String TAG = "RhoBluetoothManagerNew";
    private static final boolean D = true;
	
	
    // Key names received from the BluetoothChatService Handler
    public static final String DEVICE_NAME = "DEVICE_NAME";
    public static final String TOAST = "toast";

    // Intent request codes
    private static final int REQUEST_CONNECT_DEVICE = 958061;
    private static final int REQUEST_ENABLE_BT = 958062;

    // Name of the connected device
    private String mDeviceName = null;
    
    private String mConnectedDeviceName = null;
    //StringBuffer mInput;
    byte[] mInputBuffer;
    int    mInputBufferSize;
 	
    // Local Bluetooth adapter
    private BluetoothAdapter mBluetoothAdapter = null;

    // Member object for the chat services
    private RhoBluetoothSession mSession = null;
    
    private String mCreateSessionCallback;
    
    //private String mRole;
	
	private Activity mActivity;
	
	public RhoBluetoothManagerNew () {
		if(D) Log.d(TAG, "RhoBluetoothManagerNew()");
	}
	
	public void init() {
		// constructor
		mActivity = RhodesActivity.getInstance();
		mDeviceName = "NONAME";
		
		mInputBuffer = new byte[1024];
		mInputBufferSize = 0;
		
		
		mBluetoothIsEnabled = false;
		// check feature
		PackageManager pm = RhodesService.getInstance().getPackageManager();
		FeatureInfo[] fs = pm.getSystemAvailableFeatures();
		int i;
		for (i = 0; i < fs.length; i++) {
			if (fs[i] != null)
				if (fs[i].name != null)
					if (fs[i].name.equals(PackageManager.FEATURE_BLUETOOTH)) {
						if(D) Log.d(TAG, "sharedInstance - found Bluetooth feature in device !");
						mBluetoothIsEnabled = true;
					}
		}
		// check permission
		//if (pm.checkPermission(android.Manifest.permission.BLUETOOTH, RhodesService.getInstance().getMainActivity().getPackageName()) != PackageManager.PERMISSION_GRANTED) {
		try {
			BluetoothAdapter bluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
			if (bluetoothAdapter != null) {
				if (!bluetoothAdapter.isEnabled()) {
					if(D) Log.d(TAG, "sharedInstance - Bluetooth is not enabled !");
					mBluetoothIsEnabled = false;
				}
			}
			if(D) Log.d(TAG, "sharedInstance - Bluetooth permission is active !");
		}
		catch (SecurityException e) {
			if(D) Log.d(TAG, "sharedInstance - Bluetooth permission is INACTIVE !");
			mBluetoothIsEnabled = false;
		}
		
		
		//mInput = new StringBuffer("");
		
		
        // Get local Bluetooth adapter
		if (mBluetoothIsEnabled) {
			mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
        
        	if (mBluetoothAdapter != null) { 
            	if (mBluetoothAdapter.isEnabled()) {
            		if (mSession == null) {
                		setupSession();
                	}
                	mDeviceName = mBluetoothAdapter.getName();
            	}
			}
		}
	}
	
	public void setDeviceName(String device_name) {
		mDeviceName = device_name;
        if (mBluetoothAdapter != null) {
        	mBluetoothAdapter.setName(device_name);
        }
	}
	
    /**
     * Sends a message.
     * @param message  A string of text to send.
     */
    private void sendMessage(String message) {
        if(D) Log.i(TAG, "sendMessage");
        // Check that we're actually connected before trying anything
        if (mSession.getState() != RhoBluetoothSession.STATE_CONNECTED) {
            //Toast.makeText(this, R.string.not_connected, Toast.LENGTH_SHORT).show();
            if(D) Log.i(TAG, "NOT CONNECTED");
            return;
        }

        // Check that there's actually something to send
        if (message.length() > 0) {
            // Get the message bytes and tell the BluetoothChatService to write
            byte[] send = message.getBytes();
            mSession.write(send);

         }
    }
	
    /**
     * Sends a message.
     * @param message  A string of text to send.
     */
    private void sendData(byte data[]) {
        if(D) Log.i(TAG, "sendMessage");
        // Check that we're actually connected before trying anything
        if (mSession.getState() != RhoBluetoothSession.STATE_CONNECTED) {
            //Toast.makeText(this, R.string.not_connected, Toast.LENGTH_SHORT).show();
            if(D) Log.i(TAG, "NOT CONNECTED");
            return;
        }

        // Check that there's actually something to send
        if (data.length > 0) {
            // Get the message bytes and tell the BluetoothChatService to write
            mSession.write(data);
         }
    }

    
    
   private void setupSession() {
        Log.d(TAG, "setupSession()");

        mSession = new RhoBluetoothSession(mActivity, null);
        
     }


   public void onSessionConnectedOK() {
       //mInput.setLength(0);
	   mInputBufferSize = 0;
       PerformOnUiThread.exec( new Runnable() {
			public void run() {
				fireCreateSessionCallback(RhoBluetoothManager.BTC_OK, mConnectedDeviceName);
			}
	   },false);
   }
   
   public void onSessionDisconnected() {
	   PerformOnUiThread.exec( new Runnable() {
			public void run() {
				if (sharedInstance().getSession().getCallbackURL() == null) {
					fireCreateSessionCallback(RhoBluetoothManager.BTC_ERROR, "");
				}
				else {
					fireSessionCallback(mConnectedDeviceName, RhoBluetoothSession.BT_SESSION_DISCONNECT);
				}
			}
	   },false);
   }

   public void onSessionReadMessage(byte[] buf, int bytes) {
	   //final String message = new String(buf, 0, bytes);
	   final byte[] tmpbuf = buf.clone();
	   final int tmpbuf_length = bytes;
	   PerformOnUiThread.exec( new Runnable() {
		    private byte[] buf = tmpbuf;
		    private int buf_length = tmpbuf_length;
			public void run() {
		       //mInput.append(message);
			   int i;
			   synchronized (mInputBuffer) {
				   if (mInputBuffer.length < (mInputBufferSize + buf_length)) {
					   // need alloc new larger buffer
					   byte[] new_buf = new byte[mInputBufferSize + buf_length + 1024];
					   // copy old to new
					   for (i = 0; i < mInputBufferSize; i++) {
						   new_buf[i] = mInputBuffer[i];
					   }
					   mInputBuffer = new_buf;
				   }
				   for (i = mInputBufferSize; i < (mInputBufferSize + buf_length); i++) {
					   mInputBuffer[i] = buf[i-mInputBufferSize];
				   }
				   mInputBufferSize = mInputBufferSize + buf_length;
			   }
		       fireSessionCallback(mConnectedDeviceName, RhoBluetoothSession.BT_SESSION_INPUT_DATA_RECEIVED);
			}
	   },false);
   }
   
   public void onSessionConnectedDeviceName(String name) {
	   mConnectedDeviceName = name;
   }
   
   public void onSessionToast(String message) {
	   if (D) Log.d(TAG, "TOAST: " + message);
   }
   
 
    public void onDeviceListActivityFinished( boolean is_ok, String adress) {
    	if (is_ok) {
    		BluetoothDevice device = mBluetoothAdapter.getRemoteDevice(adress);
    		// Attempt to connect to the device
        	mSession.connect(device);
    	}
    }
    
    public void onActivityResultPrivate(int requestCode, int resultCode, Intent data) {
        if(D) Log.d(TAG, "onActivityResult " + resultCode);
        switch (requestCode) {
        case REQUEST_CONNECT_DEVICE:
            // When DeviceListActivity returns with a device to connect
            if (resultCode == Activity.RESULT_OK) {
                // Get the device MAC address
                String address = data.getExtras()
                                     .getString(RhoBluetoothDeviceListActivity.EXTRA_DEVICE_ADDRESS);
                // Get the BLuetoothDevice object
                BluetoothDevice device = mBluetoothAdapter.getRemoteDevice(address);
                // Attempt to connect to the device
                mSession.connect(device);
            }
            break;
        case REQUEST_ENABLE_BT:
            // When the request to enable Bluetooth returns
            if (resultCode == Activity.RESULT_OK) {
                // Bluetooth is now enabled, so set up a chat session
                setupSession();
            } else {
                // User did not enable Bluetooth or an error occured
                Log.d(TAG, "BT not enabled");
                //Toast.makeText(this, R.string.bt_not_enabled_leaving, Toast.LENGTH_SHORT).show();
                //finish();
         	   PerformOnUiThread.exec( new Runnable() {
         		   public void run() {
     					fireCreateSessionCallback(RhoBluetoothManager.BTC_ERROR, "");
         		   }
         	   },false);
            }
        }
    }
    
	   
	   
	public RhoBluetoothManagerNew sharedInstance() {
		return (RhoBluetoothManagerNew)RhoBluetoothManager.sharedInstance();
	}

	public RhoBluetoothSession getSession() {
		return mSession;
	}
	
	
	private void startClient() {
		PerformOnUiThread.exec( new Runnable() {
			public void run() {
		    	if(D) Log.i(TAG, "startClient");
		        Intent serverIntent = new Intent(mActivity, RhoBluetoothDeviceListActivity.class);
		        //mActivity.startActivityForResult(serverIntent, REQUEST_CONNECT_DEVICE);
		        RhodesService.getInstance().startActivity(serverIntent);
			}
		},false);
	}
	
	private void startServer() {
		PerformOnUiThread.exec( new Runnable() {
			public void run() {
		    	if(D) Log.i(TAG, "startServer");
		        if (mBluetoothAdapter.getScanMode() !=
		            BluetoothAdapter.SCAN_MODE_CONNECTABLE_DISCOVERABLE) {
		            Intent discoverableIntent = new Intent(BluetoothAdapter.ACTION_REQUEST_DISCOVERABLE);
		            discoverableIntent.putExtra(BluetoothAdapter.EXTRA_DISCOVERABLE_DURATION, 300);
		            RhodesService.getInstance().startActivity(discoverableIntent);
		        }
			}
		},false);
	}
	
	public int is_bluetooth_available() {
		if (!mBluetoothIsEnabled) return 0;
		return 1;
	}
	
	public void off_bluetooth() {
		if (!mBluetoothIsEnabled) return;
		//TODO
		if (sharedInstance().getSession() != null) {
			sharedInstance().getSession().stop();
		}
	}
	
	public void set_device_name(String device_name) {
		if (!mBluetoothIsEnabled) return;
    	if(D) Log.i(TAG, "set_device_name() : "+device_name);
    	sharedInstance().setDeviceName(device_name);
	}

	public String get_device_name() {
		if (!mBluetoothIsEnabled) return "bluetooth is not worked";
    	if(D) Log.i(TAG, "get_device_name() : "+sharedInstance().mDeviceName);
		return sharedInstance().mDeviceName;
	}
	
	public String get_last_error() {
		if (!mBluetoothIsEnabled) return RhoBluetoothManager.BTC_ERROR;
		return RhoBluetoothManager.BTC_OK;
	}
	
	public String create_session(String role, String callback_url) {
		if (!mBluetoothIsEnabled) return RhoBluetoothManager.BTC_ERROR;
    	if(D) Log.i(TAG, "create_session("+role+", "+callback_url+");");

		sharedInstance().mCreateSessionCallback = callback_url;
		//sharedInstance().mRole = role;
		if (role.equalsIgnoreCase(RhoBluetoothManager.BT_ROLE_SERVER)) {
			sharedInstance().startServer();
		}
		else {
			sharedInstance().startClient();
		}
		return RhoBluetoothManager.BTC_OK;
	}
	
	public void fireCreateSessionCallback(String status, String connected_device_name) {
    	if(D) Log.i(TAG, "fireCreateSessionCallback");
		StringBuffer body = new StringBuffer();
		body.append("&status=");
		body.append(status);
		body.append("&connected_device_name=");
		body.append(connected_device_name);
		if (mCreateSessionCallback != null) {
			RhoBluetoothManager.onCallback(mCreateSessionCallback, body.toString());
		}
	}

	
	public void session_set_callback(String connected_device_name, String callback_url) {
		if (!mBluetoothIsEnabled) return;
		if(D) Log.i(TAG, "session_set_callback : "+callback_url);
		sharedInstance().getSession().setCallbackURL(callback_url);
	}
	
	public void fireSessionCallback(String connected_device_name, String event_type) {
    	if(D) Log.i(TAG, "fireSessionCallback");
		StringBuffer body = new StringBuffer();
		body.append("&connected_device_name=");
		body.append(connected_device_name);
		body.append("&event_type=");
		body.append(event_type);
		if (mSession.getCallbackURL() != null) {
			RhoBluetoothManager.onCallback(mSession.getCallbackURL(), body.toString());
		}
	}
	

	public void session_disconnect(String connected_device_name) {
		if (!mBluetoothIsEnabled) return;
		if(D) Log.i(TAG, "session_disconnect");
		//TODO
		if (sharedInstance().getSession() != null) {
			sharedInstance().getSession().stop();
		}
	}
	
	public int session_get_status(String connected_device_name) {
		if (!mBluetoothIsEnabled) return -1;
		if(D) Log.i(TAG, "session_get_status");
		//if (sharedInstance().mInputStringsArrayAdapter.isEmpty()) {
		return sharedInstance().mInputBufferSize;//mInput.length();
	}

	public String session_read_string(String connected_device_name) {
		if (!mBluetoothIsEnabled) return RhoBluetoothManager.BTC_ERROR;
		if(D) Log.i(TAG, "session_read_string");
    	//String t = sharedInstance().mInput.toString();
    	//sharedInstance().mInput.setLength(0);
		String t = null;
	    synchronized (mInputBuffer) {
	    	t = new String(mInputBuffer, 0, mInputBufferSize);
	    	mInputBufferSize = 0;
	    }
		return t;
	}

	public void session_write_string(String connected_device_name, String str) {
		if (!mBluetoothIsEnabled) return;
		if(D) Log.i(TAG, "session_write_string");
		sharedInstance().sendMessage(str);
	}
	
	public int session_read_data(String connected_device_name, byte[] buf, int max_length) {
		if (!mBluetoothIsEnabled) return 0;
		if ((buf == null) || (max_length == 0)) {
			return mInputBufferSize;
		}
		int real_readed = 0;
		int i;
		synchronized (mInputBuffer) {
			// read;
			real_readed = mInputBufferSize;
			if (real_readed > max_length) {
				real_readed = max_length;
			}
			// copy
			for (i = 0; i < real_readed; i++) {
				buf[i] = mInputBuffer[i];
			}
			// adjust input buf
			if (mInputBufferSize > max_length) {
				for (i = 0; i < (mInputBufferSize - max_length); i++) {
					mInputBuffer[i] = mInputBuffer[i + max_length];
				}
				mInputBufferSize = mInputBufferSize - max_length;
			}
			else {
				mInputBufferSize = 0;
			}
		}
		return real_readed;
	}

	public void session_write_data(String connected_device_name, byte[] buf, int length) {
		if (!mBluetoothIsEnabled) return;
		if(D) Log.i(TAG, "session_write_data()");
		if (D) {
			int i;
			for (i = 0; i < length; i++) {
				Log.i(TAG, "     writed data ["+String.valueOf(i)+"] = " + String.valueOf(buf[i]));
			}
		}
		sharedInstance().sendData(buf);
	}



}
