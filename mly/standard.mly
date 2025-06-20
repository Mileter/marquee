// STANDARD MARQUEE LAYOUT FILE (.mly)
// This is a comment. It must be on a newlines, seperate from commands.
// It is started if the first character is a slash '/'. The second slash
// in this file is just a stylistic choice.

// Metadata commands must be run in this order; If not, the layout file is invalid
LPS 2
//         Number of lines per screenfull
SW  600
//  ^^^    Screen width, px
SH  80
//  ^^     Screen height, px

SC  3
//  ^      Count of screens
SD  500
//  ^^^    Delay after marquee stop scrolling to move to next screen (ms)
CD  1500
//  ^^^^   Delay after non-scrolling screen is displayed before moving to the next scene. (ms)
TPF 50
//  ^^     OPTIONAL, millis per frame
PM  3
//  ^      OPTIONAL, pixel movement per frame


// Font size should be such that a full character space takes of the screen height divided
// by the lines per screenful. Display in MingLiU. If MingLiU is not avaliable, use another
// monospace font as backup.

// Text segments are ordered. They will be displayed in this order as listed in layout file.
START
// ^^      starts a text segment
Hello, `FF0000:world!'
//      ^^^^^^          Color components (r, g, b) hex
//            ^         Color, and colored text seperator
//             ^^^^^^   Actual text
//     ^             ^  Open/Close Brackets
//                      If the text is short enough to fit within the window, do not scroll. Use
//                      the delay after non-scrolling screen (CD). Center this text. Apply this
//                      seperately from the rest of the screen.
This `00FF00:is so `0000FF:colorful'...'!
//                      The "`" and "'" are brackets for text manipulation. Local specification
//                      overrules global specifications.  If there are 6 characters 0-F, then the
//                      color is specified. ":" seperates the filter parameters from the text
//                      manipulated. For programmers who want to implement this, it is recommanded
//                      that a stack of some kind is used.
END
//^        ends a text segment

START

// ^ Empty lines within text segments are treated as text lines.
\\\\ \'\' hi every one \'\' \/\/
//   ^^ To use characters with special meaning as plain characters, the character must be
//      escaped using a backslash '\'.
END
//   Each segment must have exactly the amount found in the screen count parameter.
//   If this is not satisfied, the layout file is null.

START
This is a UTF8 Test. Please standby while this message is scrolling.
你如果可以看到這句話，實驗就成功了。
// ^^ CHT Text, testing UTF-8
END