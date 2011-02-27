    //
//  TabbedMainView.m
//  rhorunner
//
//  Created by Dmitry Moskalchuk on 26.03.10.
//  Copyright 2010 __MyCompanyName__. All rights reserved.
//

#import "LeftViewController.h"
#import "SimpleMainView.h"
#import "Rhodes.h"
#import "AppManager.h"

#include "common/RhodesApp.h"
#include "logging/RhoLog.h"

#include "NativeBar.h"

#undef DEFAULT_LOGCATEGORY
#define DEFAULT_LOGCATEGORY "LeftViewController"



@interface RhoLeftItem : NSObject {
@public
    NSString *url;
    UIImage* image;
    NSString *title;
}

@property (retain) NSString *url;
@property (retain) UIImage *image;
@property (retain) NSString *title;

- (id)init;
- (void)dealloc;

@end

@implementation RhoLeftItem

@synthesize url, image, title;

- (id)init {
    url = nil;
    image = nil;
    title = nil;
    return self;
}

- (void)dealloc {
    [url release];
    [image release];
    [title release];
    [super dealloc];
}

@end



@implementation LeftViewController

@synthesize itemsData, preferredSize, myFont;

- (id)initWithItems:(NSDictionary*)bar_info parent:(SplittedMainView*)parent {
	self = [self initWithStyle:UITableViewStylePlain];
	
	splittedView = parent;
	
	
	NSArray* items = (NSArray*)[bar_info objectForKey:NATIVE_BAR_ITEMS];

    int count = [items count];

    NSMutableArray *tabs = [[NSMutableArray alloc] initWithCapacity:count];
    
    NSString *initUrl = nil;
    
	self.myFont = [UIFont fontWithName:@"Helvetica-Bold" size:20.0];
	[self.myFont release];
	
	self.preferredSize = 0;
	
    for (int i = 0; i < count; ++i) {
		NSDictionary* item = (NSDictionary*)[items objectAtIndex:i];
        
        NSString *label = (NSString*)[item objectForKey:NATIVE_BAR_ITEM_LABEL];
        NSString *url = (NSString*)[item objectForKey:NATIVE_BAR_ITEM_ACTION];
        NSString *icon = (NSString*)[item objectForKey:NATIVE_BAR_ITEM_ICON];
        
        if (!initUrl)
            initUrl = url;
        
        if (label && url && icon) {
            RhoLeftItem *td = [[RhoLeftItem alloc] init];
            td.url = url;
			td.title = label;
            
			NSString *imagePath = [[AppManager getApplicationsRootPath] stringByAppendingPathComponent:icon];
			td.image = [UIImage imageWithContentsOfFile:imagePath];
            [tabs addObject:td];

			CGSize textSize = [label sizeWithFont:myFont];
			int pref_size = td.image.size.width + textSize.width + 32;
			if (self.preferredSize < pref_size) {
				self.preferredSize = pref_size;
			}
            
			
			[td release];
        
		}
    }


    self.itemsData = tabs;
    [tabs release];
	
	self.view.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleTopMargin | UIViewAutoresizingFlexibleHeight;

	[self.tableView reloadData];
	[self setSelection:0];
	
	return self;
}

/*
- (void)loadView {
	UITableView* tv = [[UITableView alloc] initWithFrame:CGRectMake(0,0, 100, 100) style:UITableViewStylePlain];
	self.view = tv;
}
 */


- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
    // Return the number of sections.
    return 1;
}


- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    // Return the number of rows in the section.
    return [self.itemsData count];
}


// Customize the appearance of table view cells.
- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    
    static NSString *CellIdentifier = @"Cell";
    
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:CellIdentifier];
    if (cell == nil) {
        cell = [[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:CellIdentifier] autorelease];
    }
    
	cell.imageView.image = [[self.itemsData objectAtIndex:indexPath.row] image];
	cell.textLabel.text = [NSString stringWithFormat:[[self.itemsData objectAtIndex:indexPath.row] title], indexPath.section, indexPath.row];
    cell.textLabel.font = myFont; 
	
	
    return cell;
}


- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation {
	return YES;
}



- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
	int selectedItem = indexPath.row;
	[splittedView switchTab:selectedItem];
}


- (void)setSelectionCommand:(NSIndexPath*)index {

	[self.tableView selectRowAtIndexPath:index animated:YES scrollPosition:UITableViewScrollPositionNone];

}

- (void)setSelection:(int)index {
	if ((index < 0) || (index >= [self.itemsData count])) {
		return;
	}
	NSIndexPath* path = [NSIndexPath indexPathForRow:index inSection:0];
	[self performSelectorOnMainThread:@selector(setSelectionCommand:) withObject:path waitUntilDone:NO];	
}

- (int)getPreferredWidth {
	return self.preferredSize;
}


@end
