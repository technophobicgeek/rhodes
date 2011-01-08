package com.rho.db;

import j2me.lang.CharacterMe;

import com.rho.RhoConf;
import com.rho.RhoEmptyLogger;
import com.rho.RhoLogger;
import com.rho.file.*;

import net.rim.device.api.io.URI;
import net.rim.device.api.database.*;
import java.util.Vector;

public class SqliteStorage implements IDBStorage 
{
	private static final RhoLogger LOG = RhoLogger.RHO_STRIP_LOG ? new RhoEmptyLogger() : 
		new RhoLogger("SqliteStorage");
	
	private IFileAccess m_fs;
	private boolean m_bPendingTransaction = false;
	private Database m_db;
	private int m_nInsideTransaction = 0;
	private IDBCallback m_dbCallback;
	
	public SqliteStorage()
	{
		m_fs = new FileAccessBB();
	}
	
	public void deleteAllFiles(String strPath) throws Exception 
	{
		String strDbName = getNameNoExt(strPath);
		m_fs.delete(strDbName + ".data");
		m_fs.delete(strDbName + ".data-journal");
	}

	public IDBResult createResult() {
		return new SqliteResult();
	}

	void bindObject(Statement st, int i, Object obj )throws DatabaseException
	{
		if ( obj instanceof String)
			st.bind(i, ((String)obj));
		else if ( obj instanceof Integer)
			st.bind(i, ((Integer)obj).intValue());
		else if ( obj instanceof Short)
			st.bind(i, ((Short)obj).shortValue());
		else if ( obj instanceof Long)
			st.bind(i, ((Long)obj).longValue());
		else if ( obj instanceof Float)
			st.bind(i, ((Float)obj).floatValue());
		else if ( obj instanceof Double)
			st.bind(i, ((Double)obj).doubleValue());
		else if ( obj instanceof Byte)
			st.bind(i, ((Byte)obj).byteValue());
		else if ( obj instanceof byte[])
			st.bind(i, (byte[])obj );
		
	}

	public IDBResult executeSQL(String strStatement, Object[] values,
			boolean bReportNonUnique) throws DBException 
	{
		LOG.TRACE(strStatement);// + "; Values: " + values);
		
		if ( m_db == null )
			throw new RuntimeException("executeSQL: m_db == null");

		IDBResult res = null;
		
		String strStatementOrig = strStatement;
		
		try
		{
			while( strStatement != null && strStatement.length()> 0)
			{
	            int start = 0;
	            while ( start < strStatement.length() && (strStatement.charAt(start) == '\n' ||
	            		strStatement.charAt(start) == '\r' ||
	            		strStatement.charAt(start) == ';' ||
	            		CharacterMe.isWhitespace(strStatement.charAt(start) ) ) )
	            		start++;
	            if (start > 0 )
	            	strStatement = strStatement.substring(start);
	            	
				if ( strStatement == null || start >= strStatement.length() )
					break;
				
				String strCommand = strStatement.length() > 6 ? strStatement.substring(0, 6) : ""; 
				boolean bSelect = strCommand.equalsIgnoreCase("SELECT");
				Statement st = m_db.createStatement(strStatement);
				boolean bDontCloseStatement = false;
				try
				{
					st.prepare();
                	strStatement = st.getTail();
					
					for ( int i = 0; values != null && i < values.length; i++ )
					{
						bindObject(st, i+1, values[i]);
					}
					
					if ( bSelect )
					{
		                if ( res == null )
		                {
		                	res = new SqliteResult(st);
		                	bDontCloseStatement = true;
		                }
					}else
					{
						try
						{
							st.execute();
							
							if ( m_nInsideTransaction == 0 &&
								(strCommand.equalsIgnoreCase("INSERT")|| strCommand.equalsIgnoreCase("DELETE") ||
								 strCommand.equalsIgnoreCase("UPDATE") ) )
								processCallbackData();
							
						}catch(DatabaseException exc)
						{
							if ( res == null && bReportNonUnique && exc.getMessage().indexOf("constraint failed") >= 0)
								res = new SqliteResult(true);
							else
								throw exc;
						}
						
	                	if ( res == null )
	                		res = new SqliteResult(null);
					}
					
				}finally
				{
					if ( !bDontCloseStatement )
						st.close();
					
					st = null;
				}
			}
		}catch(DatabaseException exc )
		{
			LOG.ERROR("executeSQL failed. Statement: " + strStatementOrig, exc);
			throw new DBException(exc);
		}
		return res;
	}

	public boolean isTableExists(String strName)throws DBException
	{
		Object vals[] = {strName};
		IDBResult res = executeSQL("SELECT name FROM sqlite_master WHERE type='table' AND name=?", vals, false );
		boolean bRes = !res.isEnd();
		res.close();
		
		return bRes;
	}
	
	public String[] getAllTableNames() throws DBException 
	{
		IDBResult res = executeSQL("SELECT name FROM sqlite_master WHERE type='table'", null ,false );
		
		Vector arTables = new Vector();
	    for ( ; !res.isEnd(); res.next() )
	    {
	    	arTables.addElement(res.getCurData()[0]);
	    }
	    
		String[] vecTables = new String[arTables.size()]; 
		for ( int i = 0; i<arTables.size();i++)
			vecTables[i] = (String)arTables.elementAt(i);
		
	    return vecTables;
	}

	public boolean isDbFileExists(String strPath) 
	{
		String strDbName = getNameNoExt(strPath);
		return m_fs.exists(strDbName + ".data");
	}

	private String getNameNoExt(String strPath){
		int nDot = strPath.lastIndexOf('.');
		String strDbName = "";
		if ( nDot > 0 )
			strDbName = strPath.substring(0, nDot);
		else
			strDbName = strPath;
		
		return strDbName;
	}
	
	public void executeBatchSQL(String strSqlScript)throws DBException
	{
		executeSQL(strSqlScript, null, false);
	}
	
	public static void OnInsertObjectRecord(Object NEW_source_id, Object NEW_attrib )
	{
		LOG.INFO("OnInsertObjectRecord");
	}
	
	public void createTriggers() throws DBException
	{
		String strTriggers = RhoFile.readStringFromJarFile("apps/db/syncdb_java.triggers", this); 
		executeBatchSQL( strTriggers );
	}
	
	public void open(String strPath, String strSqlScript) throws DBException 
	{
		try{
			String strDbName = getNameNoExt(strPath) + ".data";
			
			//m_dbSess.setDBCallback(this);

			URI myURI = URI.create(strDbName);
			
			if ( !m_fs.exists(strDbName) )
			{
				m_db = DatabaseFactory.create(myURI);
				
				m_db.beginTransaction();
				m_nInsideTransaction++;
				try
				{
					executeBatchSQL( strSqlScript );
					createTriggers();
				}catch(DBException exc)
				{
					m_db.rollbackTransaction();
					throw exc;
				}
				m_db.commitTransaction();
			}else
				m_db = DatabaseFactory.open(myURI);
				
			if ( m_bPendingTransaction )
				m_db.beginTransaction();
			
			m_bPendingTransaction = false;
		}catch(Exception exc ){
			throw new DBException(exc);
		}finally
		{
			m_nInsideTransaction = 0;
		}
	}

	public void startTransaction() throws DBException {
		try{
			if ( m_db == null )
				m_bPendingTransaction = true;
			else
				m_db.beginTransaction();
			
			m_nInsideTransaction++;
		}catch(DatabaseException exc ){
			throw new DBException(exc);
		}
	}
	
	public void onBeforeCommit() throws DBException
	{
		processCallbackData();
	}
	
	public void commit() throws DBException {
		try{
			if ( m_db!= null )
				m_db.commitTransaction();
			
		}catch(DatabaseException exc ){
			throw new DBException(exc);
		}finally
		{
			if ( m_nInsideTransaction > 0 )
				m_nInsideTransaction--;
		}
	}
	
	public void rollback() throws DBException 
	{
		try{
			if ( m_db!= null )
				m_db.rollbackTransaction();
		}catch(DatabaseException exc ){
			throw new DBException(exc);
		}finally
		{
			if ( m_nInsideTransaction > 0 )
				m_nInsideTransaction--;
		}
	}

	public void close() throws DBException 
	{
		try{
			if ( m_db!= null )
				m_db.close();
			
			m_db = null;
		}catch(DatabaseException exc ){
			throw new DBException(exc);
		}
	}
	
	void processCallbackData() throws DBException
	{
		if ( m_dbCallback == null )
			return;
		{
			IDBResult rows2Insert = executeSQL("SELECT * FROM object_attribs_to_insert", null, false);
			if ( rows2Insert == null || rows2Insert.isEnd() )
			{
				if (rows2Insert != null)
					rows2Insert.close();
			}else
			{
				m_dbCallback.onAfterInsert("object_values", rows2Insert);
				
				m_nInsideTransaction++;
				try{
					executeSQL("DELETE FROM object_attribs_to_insert", null, false);
				}finally
				{
					m_nInsideTransaction--;
				}
			}
		}
		
		{
			IDBResult rows2Delete = executeSQL("SELECT * FROM object_attribs_to_delete", null, false);
			if ( rows2Delete == null || rows2Delete.isEnd() )
			{
				if (rows2Delete != null)
					rows2Delete.close();
			}else
			{
				m_dbCallback.onBeforeDelete("object_values", rows2Delete);
				
				m_nInsideTransaction++;
				try{
					executeSQL("DELETE FROM object_attribs_to_delete", null, false);
				}finally
				{
					m_nInsideTransaction--;
				}
			}
		}
		
		{
			IDBResult rows2Update = executeSQL("SELECT * FROM object_attribs_to_update", null, false);
			if ( rows2Update == null || rows2Update.isEnd() )
			{
				if (rows2Update != null)
					rows2Update.close();
			}else
			{
				int cols[] = {3};
				m_dbCallback.onBeforeUpdate("object_values", rows2Update, cols);
				
				m_nInsideTransaction++;
				try{
					executeSQL("DELETE FROM object_attribs_to_update", null, false);
				}finally
				{
					m_nInsideTransaction--;
				}
			}
		}
		
	}
	
	public void setDbCallback(IDBCallback callback) 
	{
		m_dbCallback = callback;
	}

}
