package com.rho.net;

import java.io.IOException;

import javax.microedition.io.Connector;
import javax.microedition.io.HttpConnection;
import javax.microedition.io.Connection;
import javax.microedition.io.SocketConnection;

import net.rim.device.api.io.SocketConnectionEnhanced;
import net.rim.device.api.system.DeviceInfo;
import net.rim.device.api.system.RadioInfo;
import com.rho.BBVersionSpecific;
import com.rho.RhoEmptyLogger;
import com.rho.RhoLogger;
import com.rho.RhoConf;
import com.rho.RhodesApp;
import com.rho.Version;
import com.rho.net.bb.BBHttpConnection;
import net.rim.device.api.servicebook.ServiceRecord;
import net.rim.device.api.servicebook.ServiceBook;

public class NetworkAccess implements INetworkAccess {

	private static final RhoLogger LOG = RhoLogger.RHO_STRIP_LOG ? new RhoEmptyLogger() : 
		new RhoLogger("NetworkAccess");

	private static String URLsuffix = "";
	private static String WIFIsuffix = "";
	private static boolean networkConfigured = false;
	private static boolean bes = true;
	private static long  m_nMaxPacketSize = 0;
	
	void checkWAP(ServiceRecord[] records)
	{
        for(int i=0; i < records.length; i++)
        {
            //Search through all service records to find the
            //valid non-Wi-Fi and non-MMS
            //WAP 2.0 Gateway Service Record.
            if (records[i].isValid() && !records[i].isDisabled())
            {

                if (records[i].getUid() != null && records[i].getUid().length() != 0)
                {
                    if ((records[i].getUid().toLowerCase().indexOf("wifi") == -1) &&
                        (records[i].getUid().toLowerCase().indexOf("mms") == -1))
                    {
                    	 	URLsuffix = ";ConnectionUID=" + records[i].getUid()+";connectionhandler=none;deviceside=true";
                    	 	networkConfigured = true;
                    	 	LOG.INFO("Found WAP2 provider. Suffix: " + URLsuffix);                    	 	
                            break;
                    }
                }
            }
        }
	}
	
	public void configure() 
	{
		networkConfigured = false;
		bes = false;
		URLsuffix = "";
		WIFIsuffix = "";
		
		String strDeviceside = ";deviceside=true";
		if ( com.rho.RhoConf.getInstance().getInt("no_deviceside_postfix") == 1 )
			strDeviceside = "";
		
		if (DeviceInfo.isSimulator()) 
		{
			URLsuffix = strDeviceside;
			WIFIsuffix = ";interface=wifi";
			networkConfigured = true;
		}else
		{
			ServiceBook sb = ServiceBook.getSB();
			if (sb != null) {
				ServiceRecord[] wifis = sb.findRecordsByCid("WPTCP");
				for ( int i = 0; i < wifis.length; i++ ){
					if (/*srs[i].isDisabled() ||*/ !wifis[i].isValid())
						continue;
					
					WIFIsuffix = ";interface=wifi";// + strDeviceside; 
						//";deviceside=true;ConnectionUID=" + 
						//wifis[i].getUid();
					
					LOG.TRACE("WIFI :" + WIFIsuffix );
					
					break;
				}
                
                checkWAP(wifis);
                
				ServiceRecord[] srs = sb.getRecords();
				// search for BIS-B transport
				for (int i = 0; i < srs.length; i++) {
					if (srs[i].isDisabled() || !srs[i].isValid())
						continue;
					if (srs[i].getCid().equals("IPPP")
							&& srs[i].getName().equals("IPPP for BIBS")) {
						LOG.INFO("SRS: CID: " + srs[i].getCid() + " NAME: " + srs[i].getName());
						
						URLsuffix = ";deviceside=false;ConnectionType=mds-public";
						networkConfigured = true;
						break;
					}
				}
				
				// search for BES transport
				for (int i = 0; i < srs.length; i++) {
					LOG.INFO("SB: " + srs[i].getName() + ";UID: " + srs[i].getUid() +
							";CID: " + srs[i].getCid() +
							";APN: " + srs[i].getAPN() + ";Descr: " + srs[i].getDataSourceId() +
							";Valid: " + (srs[i].isValid() ? "true" : "false") + 
							";Disabled: "+ (srs[i].isDisabled()? "true" : "false") );
					
					if (srs[i].isDisabled() || !srs[i].isValid())
						continue;
					if (srs[i].getCid().equals("IPPP")
							&& srs[i].getName().equals("Desktop")) {
						URLsuffix = "";
						networkConfigured = true;
						bes = true;
						break;
					}
				}
				
			}
		}
		
		String strConfPostfix = com.rho.RhoConf.getInstance().getString("bb_connection_postfix");
		if ( strConfPostfix != null && strConfPostfix.length() > 0 )
		{
			URLsuffix = strConfPostfix;
			networkConfigured = true;
		}else if (networkConfigured == false) {
			URLsuffix = strDeviceside;//";deviceside=true";
			networkConfigured = true;
		}

		LOG.INFO("Postfix: " + URLsuffix + ";Wifi: " + WIFIsuffix);
	}

	/*public IHttpConnection doLocalRequest(String strUrl, String strBody)
	{
		HttpHeaders headers = new HttpHeaders();
		headers.addProperty("Content-Type", "application/x-www-form-urlencoded");
		
		
		HttpConnection http = Utilities.makeConnection(strUrl, headers, strBody.getBytes());
		
//		RhodesApplication.getInstance().postUrl(strUrl, strBody, headers);
		
		return new BBHttpConnection(http);
	}*/
	
	public long getMaxPacketSize()
	{
		if ( WIFIsuffix != null && isWifiActive() )
			return 0;
		
		m_nMaxPacketSize = RhoConf.getInstance().getInt("bb_net_maxpacketsize_kb")*1024;
		if ( (/*DeviceInfo.isSimulator() ||*/ URLsuffix.indexOf(";deviceside=true") < 0) && m_nMaxPacketSize == 0 )
		{
			//avoid 403 error on BES/BIS
			//http://supportforums.blackberry.com/t5/Java-Development/HTTP-Error-413-when-downloading-small-files/m-p/103918
			m_nMaxPacketSize = 1024*250;
		}
		
		return m_nMaxPacketSize;
	}
	
	public boolean isWifiActive()
	{
		return BBVersionSpecific.isWifiActive();
	}
	
	public IHttpConnection connect(String url, boolean ignoreSuffixOnSim) throws IOException 
	{
		if ( RhodesApp.getInstance().isRhodesAppUrl(url) )
		{
			URI uri = new URI(url);
			return new RhoConnection(uri);
		}
		
		int fragment = url.indexOf('#');
		if (-1 != fragment) {
			url = url.substring(0, fragment);
		}

		boolean ignoreSuffix = !URI.isLocalHost(url) && ignoreSuffixOnSim && DeviceInfo.isSimulator();
		HttpConnection http = (HttpConnection)baseConnect(url, ignoreSuffix );
		return new BBHttpConnection(http);
	}

	public SocketConnection socketConnect(String strHost, int nPort) throws IOException
	{
		return socketConnect("socket", strHost, nPort);
	}

	public SocketConnection socketConnect(String proto, String strHost, int nPort) throws IOException 
	{
		boolean ignoreSuffix = DeviceInfo.isSimulator();// && proto.equals("ssl") &&
			//URLsuffix.indexOf("deviceside=true") != -1;
		String strUrl = proto + "://" + strHost + ":" + Integer.toString(nPort);
		
		return (SocketConnection)baseConnect(strUrl, ignoreSuffix);
	}
	
	void setConnectionTimeout(Connection conn, int nTimeOutMS)throws  java.io.IOException
	{
        Version.SoftVersion ver = Version.getSoftVersion();
        if ( ver.nMajor < 6 )
        {
			SocketConnectionEnhanced sce = (SocketConnectionEnhanced) conn;
			short sceOption = SocketConnectionEnhanced.READ_TIMEOUT;
			sce.setSocketOptionEx(sceOption, nTimeOutMS);
        }
	}
	
	private Connection doConnect(String urlArg, boolean bThrowIOException) throws IOException
	{
		Connection conn = null;		
		try 
		{
			String url = new String(urlArg);
            if (url.startsWith("https"))
				url += ";EndToEndDesired;RdHTTPS";

			int nTimeoutMS = RhoConf.getInstance().getInt("net_timeout")*1000;
			if (nTimeoutMS == 0)
				nTimeoutMS = 30000; //30 sec by default
			
			if ( url.indexOf(";deviceside=true") == 0 && nTimeoutMS > 0 )
				url += ";ConnectionTimeout=" + nTimeoutMS;
			
			LOG.INFO("Connect to url: " + url);
            conn = Connector.open(url, Connector.READ_WRITE, true);
            
            if ( url.indexOf(";deviceside=true") >= 0 && nTimeoutMS > 0 )
            	setConnectionTimeout(conn, nTimeoutMS);
            
		} catch (java.io.InterruptedIOException ioe) 
		{
			LOG.ERROR("Connector.open InterruptedIOException", ioe );
			if (conn != null)
				conn.close();
			conn = null;
			throw ioe;			
		} catch (IOException ioe) 
		{
			LOG.ERROR("Connector.open exception", ioe );
			
            String strMsg = ioe.getMessage(); 
			boolean bTimeout = strMsg != null && (strMsg.indexOf("timed out") >= 0 || strMsg.indexOf("Timed out") >= 0);
			boolean bDNS = strMsg != null && (strMsg.equalsIgnoreCase("Error trying to resolve") || strMsg.indexOf("DNS") >= 0);
			
			if ( bTimeout || bDNS || bThrowIOException )				
			{				
				if (conn != null)
					conn.close();
				conn = null;
				throw ioe;
			}
		}catch(Exception exc)
		{
			throw new IOException("Could not open network connection.");
		}
		
		return conn;
	}
	
	public Connection baseConnect(String strUrl, boolean ignoreSuffix) throws IOException 
	{
		Connection conn = null;
		
		//Try wifi first
		if ( WIFIsuffix != null && isWifiActive() )
		{
			conn = doConnect(strUrl + WIFIsuffix + (URLsuffix.startsWith(";ConnectionUID=")? "":URLsuffix), false);
			//if ( conn == null )
			//	conn = doConnect(strUrl + WIFIsuffix, false);				
		}
		
		if ( conn == null  )
		{
			if ( isNetworkAvailable() )
			{
				conn = doConnect(strUrl + URLsuffix, true);
				//if ( conn == null && URLsuffix != null && URLsuffix.length() > 0 )
				//	conn = doConnect(strUrl, true);
			}else
				throw new IOException("No network coverage.");				
		}
		
		return conn;
	}
	
	public void close() {
	}

	public boolean isNetworkAvailable() 
	{
		if (!(RadioInfo.getState() == RadioInfo.STATE_ON))
			return false;
		if ((RadioInfo.getNetworkService() & RadioInfo.NETWORK_SERVICE_DATA) == 0)
			return false;
		//if (bes)
		//	return true;
		//if (URLsuffix == null)
		//	return false;
		//return networkConfigured;
		return true;
	}
	
}
