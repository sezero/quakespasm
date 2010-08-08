//
//  UpdateButton.m
//  QuakeSpasm
//
//  Created by Kristian Duske on 01.08.10.
//  Copyright 2010 __MyCompanyName__. All rights reserved.
//

#import "UpdateButton.h"


@implementation UpdateButton

- (void)resetCursorRects {
	[self addCursorRect:[self bounds] cursor:[NSCursor pointingHandCursor]];
}

@end
