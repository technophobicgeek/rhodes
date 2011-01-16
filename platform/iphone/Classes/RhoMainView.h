//
//  RhoMainView.h
//  rhorunner
//
//  Created by Dmitry Moskalchuk on 07.03.10.
//  Copyright 2010 Rhomobile Inc. All rights reserved.
//

#import <UIKit/UIKit.h>

@protocol RhoMainView

- (UIView*)view;

- (UIWebView*)detachWebView;

- (void)loadHTMLString:(NSString*)data;

- (void)back:(int)index;
- (void)forward:(int)index;
- (void)navigate:(NSString*)url tab:(int)index;
- (void)navigateRedirect:(NSString*)url tab:(int)index;
- (void)reload:(int)index;

- (void)executeJs:(NSString*)js tab:(int)index;

- (NSString*)currentLocation:(int)index;

- (void)switchTab:(int)index;
- (int)activeTab;
- (void)setTabBadge:(int)index val:(char*)val;

- (void)addNavBar:(NSString*)title left:(NSArray*)left right:(NSArray*)right;
- (void)removeNavBar;

- (UIWebView*)getWebView:(int)tab_index;

-(void)openNativeView:(UIView*)nv_view tab_index:(int)tab_index;
-(void)closeNativeView:(int)tab_index;

@end
