//
//  RhoNativeViewManagerOC.h
//  rhorunner
//
//  Created by Dmitry Soldatenkov on 8/25/10.
//  Copyright 2010 __MyCompanyName__. All rights reserved.
//

#import <UIKit/UIKit.h>
//#import <NSMapTable.h>


#include "ruby/ext/rho/rhoruby.h"


//#define   OC_OPEN_IN_MODAL_FULL_SCREEN_WINDOW  11111


@protocol NativeViewOC

- (UIView*)getView;
- (void)navigate:(NSString*)url;

@end

@protocol NativeViewFactoryOC 

-(id)getNativeView:(NSString*)viewType;	
-(void)destroyNativeView:(id)nativeView;

@end

@interface RhoNativeViewManagerOC : NSObject {
	NSMutableDictionary* mProviders;
	NSMutableArray* mOpenedViews;
	
}

@property (nonatomic, retain) NSMutableDictionary *mProviders;
@property (nonatomic, retain) NSMutableArray* mOpenedViews;

- (id)init;
- (void)dealloc;	
	
+ (id)getNativeView:(NSString*)viewType;
+ (void)destroyNativeView:(id)nativeView;


+(void)registerViewType:(NSString*)viewType factory:(id)factory;
+(void)unregisterViewType:(NSString*)viewType;
+(UIWebView*)getWebViewObject:(int)tab_index;

+(int)create_native_view:(NSString*)viewType tab_index:(int)tab_index params:(VALUE)params;
+(void)navigate_native_view:(int)nv_id message:(NSString*)message;
+(void)destroy_native_view:(int)nv_id;
+(void)destroy_native_view_by_nview:(void*)nv_view;

@end


