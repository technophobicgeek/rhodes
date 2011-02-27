package com.rhomobile.rhodes.event;

import java.util.Date;
import java.util.Vector;

import android.database.Cursor;
import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.ContentValues;
import android.net.Uri;
import android.os.Build;

import com.rhomobile.rhodes.Capabilities;
import com.rhomobile.rhodes.Logger;
import com.rhomobile.rhodes.RhodesActivity;
import com.rhomobile.rhodes.RhodesService;

public class EventStore {
	
	private static final String TAG = "EventStore";
	
	private static final String AUTHORITY = Build.VERSION.SDK_INT >= Build.VERSION_CODES.FROYO ? "com.android.calendar" : "calendar";
	private static final Uri EVENTS_URI = Uri.parse("content://" + AUTHORITY + "/events");
	
	private static final String EVENTS_TITLE = "title";
	private static final String EVENTS_START_DATE = "dtstart";
	private static final String EVENTS_END_DATE = "dtend";
	private static final String EVENTS_LOCATION = "eventLocation";
	private static final String EVENTS_NOTES = "description";
	private static final String EVENTS_PRIVACY = "visibility";
	
	private static void reportFail(String name, Exception e) {
		Logger.E(TAG, "Call of \"" + name + "\" failed: " + e != null ? e.getMessage() : "null exception");
		if ( e != null )
		    e.printStackTrace();
	}
	
	private static String dateToString(Date date) {
		return String.format("%04d-%02d-%02d %02d:%02d:%02d",
				date.getYear() + 1900, date.getMonth() + 1, date.getDate(),
				date.getHours(), date.getMinutes(), date.getSeconds());
	}
	
	private static void checkCapabilities() throws IllegalAccessException {
		if (!Capabilities.CALENDAR_ENABLED)
			throw new IllegalAccessException("Capability CALENDAR disabled");
	}
	
	public static boolean hasCalendar() {
		if (!Capabilities.CALENDAR_ENABLED) {
			Logger.E(TAG, "Calendar capability is not enabled !!!");
			return false;
		}
		final Cursor calendarCursor = getContentResolver().query(
				Uri.parse("content://" + AUTHORITY + "/calendars"),
				new String[] {"_id"},
				null, null, null);
		try {
			if (calendarCursor != null) {
				if (calendarCursor.moveToFirst())
					return true;
			}
		}
		finally {
			if (calendarCursor != null) {
				calendarCursor.close();
			}
		}
		return false;
	}
	
	private static ContentResolver getContentResolver() {
		return RhodesService.getInstance().getContentResolver();
	}
	
	private static long getDefaultCalendarId() {
		final Cursor calendarCursor = getContentResolver().query(
				Uri.parse("content://" + AUTHORITY + "/calendars"),
				new String[] {"_id"},
				null, null, null);
		try {
			if (!calendarCursor.moveToFirst())
				throw new RuntimeException("No calendars found!");
			long id = calendarCursor.getLong(0);
			return id;
		}
		finally {
			calendarCursor.close();
		}
	}
	
	public static Object fetch(Date startDate, Date endDate, boolean includeRepeating) {
		try {
			checkCapabilities();
			
			Logger.D(TAG, "fetch(start, end), start: " + dateToString(startDate) + ", end: " + dateToString(endDate));
			
			Vector<Event> ret = new Vector<Event>();
			
			ContentResolver r = getContentResolver();
			
			Cursor eventCursor;
			if (includeRepeating) {
				Uri.Builder builder = Uri.parse("content://" + AUTHORITY + "/instances/when").buildUpon();
				ContentUris.appendId(builder, startDate.getTime());
				ContentUris.appendId(builder, endDate.getTime());
				
				eventCursor = r.query(builder.build(),
						new String[] {"event_id", EVENTS_TITLE, "begin", "end", EVENTS_LOCATION,
							EVENTS_NOTES, EVENTS_PRIVACY, "deleted"},
						null, //"Calendars._id=" + id,
						null, "startDay ASC, startMinute ASC");
			}
			else {
				String where = String.format("(%s >= ? and %s <= ?) or (%s >= ? and %s <= ?)",
						EVENTS_START_DATE, EVENTS_START_DATE, EVENTS_END_DATE, EVENTS_END_DATE);
				String start = Long.toString(startDate.getTime());
				String end = Long.toString(endDate.getTime());
				eventCursor = r.query(EVENTS_URI,
					new String[] {"_id", EVENTS_TITLE, EVENTS_START_DATE, EVENTS_END_DATE,
						EVENTS_LOCATION, EVENTS_NOTES, EVENTS_PRIVACY, "deleted"},
					where, new String[] {start, end, start, end},
					null);
			}
			if (eventCursor == null)
				throw new RuntimeException("Calendar provider not found");
			try {
				while (eventCursor.moveToNext()) {

					int deleted_index = eventCursor.getColumnIndex("deleted");
					if (deleted_index >= 0) {
						long is_deleted = eventCursor.getLong(deleted_index);
						if (is_deleted != 0) {
							continue;
						}
					}
					
					String eid = eventCursor.getString(0);
					Event event = new Event(eid);
					
					Date eventStartDate = new Date(eventCursor.getLong(2));
					Date eventEndDate = new Date(eventCursor.getLong(3));
					if (eventStartDate.after(eventEndDate)) {
						Date tmp = eventStartDate;
						eventStartDate = eventEndDate;
						eventEndDate = tmp;
					}
					
					if (eventEndDate.before(startDate) || eventStartDate.after(endDate))
						continue;
					
					event.title = eventCursor.getString(1);
					event.startDate = eventStartDate;
					event.endDate = eventEndDate;
					event.location = eventCursor.getString(4);
					event.notes = eventCursor.getString(5);
					switch (eventCursor.getInt(6)) {
					case 1: event.privacy = "confidential"; break;
					case 2: event.privacy = "private"; break;
					case 3: event.privacy = "public"; break;
					}
					
					Logger.D(TAG, "Event: id: " + event.id +
							", title: " + event.title +
							", begin: " + dateToString(event.startDate) +
							", end: " + dateToString(event.endDate));
					
					ret.add(event);
				}
			}
			finally {
				eventCursor.close();
			}
			
			return ret;
		}
		catch (Exception e) {
			reportFail("fetch(start, end)", e);
			String error = e.getMessage();
			return error == null ? "unknown" : error;
		}
	}
	
	public static Object fetch(String id) {
		try {
			checkCapabilities();
			
			Logger.D(TAG, "fetch(id)");
			
			ContentResolver r = getContentResolver();
			
			Uri uri = ContentUris.withAppendedId(EVENTS_URI, Long.parseLong(id));
			final Cursor eventCursor = r.query(uri,
					new String[] {EVENTS_TITLE, EVENTS_START_DATE, EVENTS_END_DATE,
						EVENTS_LOCATION, EVENTS_NOTES, EVENTS_PRIVACY},
					null, null, null);
			if (eventCursor == null)
				throw new RuntimeException("Calendar provider not found");
			
			try {
				if (!eventCursor.moveToFirst()) {
					Logger.D(TAG, "fetch(id): result set is empty");
					return null;
				}
				
				Event event = new Event(id);
				event.title = eventCursor.getString(0);
				event.startDate = new Date(eventCursor.getLong(1));
				event.endDate = new Date(eventCursor.getLong(2));
				event.location = eventCursor.getString(3);
				event.notes = eventCursor.getString(4);
				switch (eventCursor.getInt(5)) {
				case 1: event.privacy = "confidential"; break;
				case 2: event.privacy = "private"; break;
				case 3: event.privacy = "public"; break;
				}
				
				Logger.D(TAG, "Event: id: " + event.id +
						", title: " + event.title +
						", begin: " + dateToString(event.startDate) +
						", end: " + dateToString(event.endDate));
				
				return event;
			}
			finally {
				eventCursor.close();
			}
		}
		catch (Exception e) {
			reportFail("fetch(id)", e);
			String error = e.getMessage();
			return error == null ? "unknown" : error;
		}
	}
	
	public static String save(Event event) {
		String return_id = null;
		try {
			checkCapabilities();
			
			Logger.D(TAG, "save(event)");
			
			ContentValues values = new ContentValues();
			values.put(EVENTS_TITLE, event.title);
			values.put(EVENTS_START_DATE, event.startDate.getTime());
			values.put(EVENTS_END_DATE, event.endDate.getTime());
			if (event.location != null)
				values.put(EVENTS_LOCATION, event.location);
			if (event.notes !=  null)
				values.put(EVENTS_NOTES, event.notes);
			if (event.privacy != null)
			{
				int privacy = 0;
				if (event.privacy.equalsIgnoreCase("confidential"))
					privacy = 1;
				else if (event.privacy.equalsIgnoreCase("private"))
					privacy = 2;
				else if (event.privacy.equalsIgnoreCase("public"))
					privacy = 3;
				values.put(EVENTS_PRIVACY, privacy);
			}

			long calendarId = getDefaultCalendarId();
			values.put("calendar_id", calendarId);
			
			ContentResolver r = getContentResolver();
					
			if (event.id == null || event.id.equalsIgnoreCase("")) {
				Logger.D(TAG, "Insert new event...");
				Uri euri = r.insert(EVENTS_URI, values);
				r.notifyChange(EVENTS_URI, null);
				event.id = Long.toString(ContentUris.parseId(euri));
				Logger.D(TAG, "Event id of event is " + event.id);
				return_id = event.id;
			}
			else {
				return_id = event.id;
				Logger.D(TAG, "Update event...");
				Uri uri = ContentUris.withAppendedId(EVENTS_URI, Long.parseLong(event.id));
				r.update(uri, values, null, null);
				r.notifyChange(EVENTS_URI, null);
				Logger.D(TAG, "Event updated");
			}
		}
		catch (Exception e) {
			reportFail("save", e);
			//String error = e.getMessage();
			return null;//error == null ? "unknown" : error;
		}
		return return_id;
	}
	
	public static String delete(String id) {
		try {
			checkCapabilities();
			
			Logger.D(TAG, "delete("+id+")");
			
			ContentResolver r = getContentResolver();
			int rows;
			if (Build.VERSION.SDK_INT < Build.VERSION_CODES.FROYO) {
				Logger.D(TAG, " use old delete scheme");
				Uri uri = ContentUris.withAppendedId(EVENTS_URI, Long.parseLong(id));
				rows = getContentResolver().delete(uri, null, null);
			}
			else {
				Logger.D(TAG, " use new delete scheme");
				int idn = Integer.decode(id).intValue();
				Uri du = Uri.withAppendedPath(EVENTS_URI, id);
				rows = r.delete(du, null, null);
				//rows = r.delete(EVENTS_URI, "_id=?", new String[] {id});
				r.notifyChange(EVENTS_URI, null);
			}
			Logger.D(TAG, String.format("%d rows deleted", rows));
			
			return null;
		}
		catch (Exception e) {
			reportFail("delete", e);
			String error = e.getMessage();
			return error == null ? "unknown" : error;
		}
	}
	
}
