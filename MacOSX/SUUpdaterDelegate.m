/*
 Copyright (C) 2007-2008 Kristian Duske
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 
 See the GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 
 */

#import "SUUpdaterDelegate.h"

@implementation SUUpdaterDelegate

- (id)initWithTabView:(NSTabView *)tabView 
            indicator:(NSProgressIndicator *)indicator 
               button:(NSButton *)button 
                label:(NSTextField *)label{
	if (tabView == nil || indicator == nil || button == nil || label == nil) {
		[self release];
		return nil;
	}
	
	if (self = [super init]) {
		updateTabView = [tabView retain];
		updateProgressIndicator = [indicator retain];
		updateButton = [button retain];
        versionLabel = [label retain];
	}
	
	return self;
}

- (void)updater:(SUUpdater *)updater didFindValidUpdate:(SUAppcastItem *)update {
	[updateProgressIndicator stopAnimation:updater];
	[updateButton setTitle:[NSString stringWithFormat:@"New version available: %@", [update displayVersionString]]];

	NSMutableAttributedString *colorTitle = [[NSMutableAttributedString alloc] initWithAttributedString:[updateButton attributedTitle]];
	[colorTitle addAttribute:NSForegroundColorAttributeName
					   value:[NSColor blueColor]
					   range:NSMakeRange(0, [colorTitle length])];
	[updateButton setAttributedTitle:colorTitle];
	
	[updateButton sizeToFit];
	[updateTabView selectTabViewItemAtIndex:1];
}

- (void)updaterDidNotFindUpdate:(SUUpdater *)updater {
    NSBundle* bundle = [NSBundle mainBundle];
    NSString* version = [NSString stringWithFormat:@"Version %@", [bundle objectForInfoDictionaryKey:@"CFBundleVersion"], nil];
    [versionLabel setStringValue:version];
    [versionLabel sizeToFit];
    
	[updateProgressIndicator stopAnimation:updater];
	[updateTabView selectTabViewItemAtIndex:2];
}


- (void)dealloc {
	[updateTabView release];
	[updateProgressIndicator release];
	[updateButton release];
    [versionLabel release];
	[super dealloc];
}

@end
