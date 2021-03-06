/*
   Fibonacci v3D: https://github.com/evilgeniuslabs/fibonacci-v3d
   Copyright (C) 2014-2016 Jason Coon, Evil Genius Labs

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "FastLED.h"
FASTLED_USING_NAMESPACE;

#include "application.h"
#include "math.h"

SYSTEM_THREAD(ENABLED);

#include "IRRemote-Particle.h"

#define IR_RECV_PIN D0

IRrecv irReceiver(IR_RECV_PIN);

/*#define BUTTON_1_PIN 22
#define BUTTON_2_PIN 21

Bounce button1 = Bounce();
Bounce button2 = Bounce();*/

#include "Commands.h"
#include "GradientPalettes.h"

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

#define DATA_PIN    A5
// #define COLOR_ORDER RGB
#define LED_TYPE    WS2811
#define NUM_LEDS    100

CRGB leds[NUM_LEDS];

const uint8_t brightnessCount = 5;
uint8_t brightnessMap[brightnessCount] = { 16, 32, 64, 128, 255 };

// variables exposed via Particle cloud API (Spark Core is limited to 10)
int brightness = brightnessMap[0];
int patternIndex = 0;
String patternName = "Pride";
int power = 1;
char variableValue[32] = "";

// variables exposed via the variableValue variable, via Particle Cloud API
int r = 0;
int g = 0;
int b = 255;

CRGB solidColor = CRGB::Red;

uint8_t gHue = 0; // rotating "base color" used by many of the patterns

int autoPlayDurationSeconds = 10;
unsigned int autoPlayTimout = 0;
bool autoplayEnabled = false;

CRGBPalette16 IceColors_p = CRGBPalette16(CRGB::Black, CRGB::Blue, CRGB::Aqua, CRGB::White);

uint8_t paletteIndex = 0;

// List of palettes to cycle through.
CRGBPalette16 palettes[] =
{
  RainbowColors_p,
  RainbowStripeColors_p,
  CloudColors_p,
  OceanColors_p,
  ForestColors_p,
  HeatColors_p,
  LavaColors_p,
  PartyColors_p,
  IceColors_p,
};

uint8_t paletteCount = ARRAY_SIZE(palettes);

CRGBPalette16 currentPalette(CRGB::Black);
CRGBPalette16 targetPalette = palettes[paletteIndex];

// ten seconds per color palette makes a good demo
// 20-120 is better for deployment
#define SECONDS_PER_PALETTE 10

///////////////////////////////////////////////////////////////////////

// Forward declarations of an array of cpt-city gradient palettes, and
// a count of how many there are.  The actual color palette definitions
// are at the bottom of this file.
extern const TProgmemRGBGradientPalettePtr gGradientPalettes[];
extern const uint8_t gGradientPaletteCount;

// Current palette number from the 'playlist' of color palettes
uint8_t gCurrentPaletteNumber = 0;

CRGBPalette16 gCurrentPalette( CRGB::Black);
CRGBPalette16 gTargetPalette( gGradientPalettes[0] );

uint8_t fibonacciToPhysicalOrder[100] = {
  99, 97, 98, 96, 92, 95, 91, 93, 89, 84,
  94, 90, 85, 88, 83, 86, 81, 76, 87, 82,
  77, 80, 75, 78, 73, 68, 79, 74, 69, 72,
  67, 70, 65, 60, 71, 66, 61, 64, 59, 62,
  57, 52, 63, 58, 53, 56, 51, 54, 48, 41,
  55, 50, 43, 47, 40, 45, 49, 42, 46, 39,
  44, 36, 28, 33, 38, 30, 35, 27, 32, 37,
  29, 34, 26, 31, 23, 15, 20, 25, 17, 22,
  14, 19, 24, 16, 21, 13, 18, 10,  2,  7,
  12,  4,  9,  1,  6, 11,  3,  8,  0,  5
};

uint8_t physicalToFibonacciOrder[100] = {
  98, 93, 88, 96, 91, 99, 94, 89, 97, 92,
  87, 95, 90, 85, 80, 75, 83, 78, 86, 81,
  76, 84, 79, 74, 82, 77, 72, 67, 62, 70,
  65, 73, 68, 63, 71, 66, 61, 69, 64, 59,
  54, 49, 57, 52, 60, 55, 58, 53, 48, 56,
  51, 46, 41, 44, 47, 50, 45, 40, 43, 38,
  33, 36, 39, 42, 37, 32, 35, 30, 25, 28,
  31, 34, 29, 24, 27, 22, 17, 20, 23, 26,
  21, 16, 19, 14,  9, 12, 15, 18, 13,  8,
  11,  6,  4,  7, 10,  5,  3,  1,  2,  0
};

// Params for width and height
const uint8_t kMatrixWidth = 32;
const uint8_t kMatrixHeight = 32;

const uint8_t maxX = kMatrixWidth - 1;
const uint8_t maxY = kMatrixHeight - 1;

const uint8_t coordsX10[NUM_LEDS] = { 5, 4, 5, 5, 4, 5, 4, 4, 6, 3, 5, 5, 3, 6, 4, 4, 6, 3, 6, 4, 3, 7, 3, 5, 6, 2, 7, 4, 4, 7, 2, 6, 5, 2, 7, 3, 5, 6, 2, 7, 4, 3, 7, 2, 6, 6, 2, 8, 3, 4, 7, 1, 7, 5, 2, 8, 2, 5, 6, 1, 8, 3, 3, 8, 1, 6, 5, 1, 8, 2, 4, 7, 1, 7, 4, 2, 8, 1, 6, 6, 1, 8, 3, 3, 8, 0, 7, 5, 1, 9, 1, 5, 7, 0, 8, 3, 2, 9, 0, 6 };

const uint8_t coordsY10[NUM_LEDS] = { 5, 4, 5, 4, 5, 5, 3, 6, 4, 4, 6, 3, 5, 5, 3, 6, 3, 4, 6, 2, 6, 4, 3, 7, 3, 5, 5, 2, 7, 3, 4, 7, 2, 6, 5, 3, 7, 2, 5, 6, 2, 7, 4, 3, 7, 2, 6, 5, 2, 8, 3, 4, 7, 1, 7, 4, 2, 8, 2, 5, 6, 1, 8, 3, 3, 8, 1, 7, 5, 2, 8, 2, 5, 7, 1, 8, 4, 2, 8, 1, 6, 6, 1, 8, 2, 4, 8, 0, 7, 5, 1, 9, 1, 5, 7, 0, 8, 3, 3, 9 };

const uint8_t coordsX32[NUM_LEDS] = { 17, 15, 16, 18, 13, 19, 15, 14, 20, 11, 18, 18, 11, 22, 12, 15, 21, 9, 21, 16, 11, 23, 10, 18, 20, 8, 24, 13, 13, 24, 7, 21, 18, 9, 25, 9, 16, 23, 6, 24, 14, 10, 26, 6, 20, 20, 6, 27, 10, 14, 25, 4, 24, 16, 8, 28, 7, 18, 23, 4, 27, 12, 11, 28, 4, 22, 19, 5, 29, 8, 15, 26, 2, 26, 15, 8, 30, 4, 20, 23, 2, 30, 10, 12, 29, 1, 25, 18, 4, 31, 5, 17, 26, 0, 29, 12, 8, 31, 1, 22 };

const uint8_t coordsY32[NUM_LEDS] = { 16, 15, 19, 14, 17, 18, 12, 20, 14, 14, 21, 11, 19, 17, 11, 22, 12, 16, 21, 9, 22, 15, 12, 24, 9, 18, 20, 8, 24, 12, 14, 24, 7, 22, 17, 9, 26, 9, 17, 22, 6, 25, 13, 11, 26, 6, 21, 19, 6, 27, 9, 15, 25, 4, 25, 15, 8, 28, 6, 19, 22, 4, 28, 11, 12, 28, 3, 23, 18, 5, 30, 7, 16, 25, 2, 27, 13, 8, 30, 3, 21, 21, 3, 30, 8, 13, 28, 1, 26, 16, 5, 31, 4, 18, 25, 1, 30, 11, 9, 31 };

const uint8_t coordsX[NUM_LEDS] = { 137, 116, 130, 143, 101, 154, 119, 112, 164, 91, 146, 141, 88, 175, 100, 121, 168, 74, 168, 125, 90, 188, 77, 142, 160, 66, 189, 102, 105, 190, 59, 167, 141, 69, 204, 75, 128, 181, 48, 193, 113, 83, 210, 52, 158, 161, 48, 213, 82, 109, 203, 36, 189, 131, 61, 225, 53, 142, 184, 30, 216, 97, 85, 223, 30, 177, 154, 39, 234, 61, 120, 208, 17, 211, 117, 60, 240, 31, 159, 180, 19, 237, 77, 94, 231, 11, 198, 142, 35, 251, 40, 135, 207, 4, 232, 99, 66, 250, 11, 179 };

const uint8_t coordsY[NUM_LEDS] = { 128, 117, 148, 109, 133, 144, 96, 160, 115, 113, 166, 86, 151, 138, 88, 179, 94, 126, 167, 71, 173, 120, 93, 189, 72, 148, 156, 65, 193, 95, 110, 189, 55, 174, 134, 71, 206, 69, 135, 177, 47, 199, 106, 89, 209, 47, 166, 154, 50, 217, 75, 116, 200, 33, 196, 123, 66, 225, 46, 150, 178, 31, 221, 88, 93, 221, 26, 186, 146, 43, 237, 53, 129, 202, 17, 218, 107, 68, 239, 25, 169, 172, 22, 242, 67, 103, 226, 7, 208, 132, 42, 252, 31, 146, 199, 5, 239, 88, 75, 247 };

const uint8_t ledAngles[NUM_LEDS] = { 0, 158, 60, 219, 121, 23, 181, 84, 242, 144, 46, 204, 107, 9, 167, 69, 227, 130, 32, 190, 92, 251, 153, 55, 213, 115, 18, 176, 78, 236, 139, 41, 199, 101, 3, 162, 64, 222, 124, 26, 185, 87, 245, 147, 50, 208, 110, 12, 170, 73, 231, 133, 35, 193, 96, 254, 156, 58, 217, 119, 21, 179, 81, 240, 142, 44, 202, 105, 7, 165, 67, 225, 128, 30, 188, 90, 248, 151, 53, 211, 113, 16, 174, 76, 234, 136, 39, 197, 99, 1, 160, 62, 220, 122, 24, 183, 85, 243, 145, 47 };

const uint8_t ledRadii[NUM_LEDS] = { 0, 3, 5, 8, 10, 13, 15, 18, 20, 23, 26, 28, 31, 33, 36, 38, 41, 44, 46, 49, 51, 54, 56, 59, 61, 64, 67, 69, 72, 74, 77, 79, 82, 84, 87, 90, 92, 95, 97, 100, 102, 105, 108, 110, 113, 115, 118, 120, 123, 125, 128, 131, 133, 136, 138, 141, 143, 146, 148, 151, 154, 156, 159, 161, 164, 166, 169, 172, 174, 177, 179, 182, 184, 187, 189, 192, 195, 197, 200, 202, 205, 207, 210, 212, 215, 218, 220, 223, 225, 228, 230, 233, 236, 238, 241, 243, 246, 248, 251, 253 };

void setPixelA(uint8_t angle, uint8_t dAngle, CRGB color)
{
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    uint8_t o = physicalToFibonacciOrder[i];

    uint8_t ao = ledAngles[o];

    if (ao <= qadd8(angle, dAngle) && ao >= qsub8(angle, dAngle)) {
      leds[i] = color;
    }
  }
}

void setPixelAR(uint8_t angle, uint8_t radius, uint8_t dAngle, uint8_t dRadius, CRGB color)
{
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    uint8_t o = physicalToFibonacciOrder[i];

    uint8_t ao = ledAngles[o];

    if (ao <= qadd8(angle, dAngle) && ao >= qsub8(angle, dAngle)) {
      uint8_t ro = ledRadii[o];

      if (ro <= qadd8(radius, dRadius) && ro >= qsub8(radius, dRadius)) {
        leds[i] = color;
      }
    }
  }
}

void setPixelXY10(uint8_t x, uint8_t y, CRGB color)
{
  if ((x >= 10) || (y >= 10)) {
    return;
  }

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    uint8_t o = physicalToFibonacciOrder[i];

    if (coordsX10[o] == x && coordsY10[o] == y) {
      leds[i] = color;
    }
  }
}

void setPixelXY(uint8_t x, uint8_t y, CRGB color)
{
  if ((x >= kMatrixWidth) || (y >= kMatrixHeight)) {
    return;
  }

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    uint8_t o = physicalToFibonacciOrder[i];

    if (coordsX32[o] == x && coordsY32[o] == y) {
      leds[i] = color;
      return;
    }
  }
}

void powerOff()
{
  // clear the display
  // fill_solid(leds, NUM_LEDS, CRGB::Black);
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Black;
    FastLED.show(); // display this frame
    delay(1);
  }

  FastLED.show(); // display this frame

  while (true) {
    InputCommand command = readCommand();
    if (command == InputCommand::Power ||
        command == InputCommand::Brightness)
      return;

    // go idle for a while, converve power
    delay(250);
  }
}

int getBrightnessLevel() {
  int level = 0;
  for (int i = 0; i < brightnessCount; i++) {
    if (brightnessMap[i] >= brightness) {
      level = i;
      break;
    }
  }
  return level;
}

void adjustBrightness(int delta) {
  int level = getBrightnessLevel();

  level += delta;
  if (level < 0)
    level = brightnessCount - 1;
  if (level >= brightnessCount)
    level = 0;

  brightness = brightnessMap[level];
  FastLED.setBrightness(brightness);

  EEPROM.write(0, brightness);
}

uint8_t cycleBrightness() {
  adjustBrightness(1);

  if (brightness == brightnessMap[0])
    return 8;

  return brightness;
}

// algorithm from http://en.wikipedia.org/wiki/Midpoint_circle_algorithm
void drawCircle(uint8_t x0, uint8_t y0, uint8_t radius, const CRGB color)
{
  int a = radius, b = 0;
  int radiusError = 1 - a;

  if (radius == 0) {
    setPixelXY(x0, y0, color);
    return;
  }

  while (a >= b)
  {
    setPixelXY(a + x0, b + y0, color);
    setPixelXY(b + x0, a + y0, color);
    setPixelXY(-a + x0, b + y0, color);
    setPixelXY(-b + x0, a + y0, color);
    setPixelXY(-a + x0, -b + y0, color);
    setPixelXY(-b + x0, -a + y0, color);
    setPixelXY(a + x0, -b + y0, color);
    setPixelXY(b + x0, -a + y0, color);

    b++;
    if (radiusError < 0)
      radiusError += 2 * b + 1;
    else
    {
      a--;
      radiusError += 2 * (b - a + 1);
    }
  }
}

// scale the brightness of all pixels down
void dimAll(byte value)
{
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i].nscale8(value);
  }
}

// Patterns from FastLED example DemoReel100: https://github.com/FastLED/FastLED/blob/master/examples/DemoReel100/DemoReel100.ino

uint8_t rainbow()
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue, 255 / NUM_LEDS);
  return 8;
}

void addGlitter( uint8_t chanceOfGlitter)
{
  if ( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}

uint8_t rainbowWithGlitter()
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
  return 8;
}

uint8_t rainbowSolid()
{
  fill_solid(leds, NUM_LEDS, CHSV(gHue, 255, 255));
  return 8;
}

uint8_t confetti()
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
  return 8;
}

uint8_t sinelon1()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16(13, 0, NUM_LEDS);
  leds[fibonacciToPhysicalOrder[pos]] += CHSV( gHue, 255, 192);
  return 8;
}

uint8_t sinelon2()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16(13, 0, NUM_LEDS);
  leds[pos] += CHSV( gHue, 255, 192);
  return 8;
}

uint8_t bpm1()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for ( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[fibonacciToPhysicalOrder[i]] = ColorFromPalette(currentPalette, gHue + (i * 2), beat - gHue + (i * 10));
  }

  return 8;
}

uint8_t bpm2()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for ( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(currentPalette, gHue + (i * 2), beat - gHue + (i * 10));
  }

  return 8;
}

uint8_t juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  uint8_t dotcount = 3;
  for ( int i = 0; i < dotcount; i++) {
    leds[beatsin16(i + (dotcount - 1), 0, NUM_LEDS)] |= CHSV(dothue, 200, 255);
    dothue += (256 / dotcount);
  }

  return 8;
}

uint8_t juggle2()
{
  static uint8_t    numdots =   4; // Number of dots in use.
  static uint8_t   faderate =   2; // How long should the trails be. Very low value = longer trails.
  static uint8_t     hueinc =  255 / numdots - 1; // Incremental change in hue between each dot.
  static uint8_t    thishue =   0; // Starting hue.
  static uint8_t     curhue =   0; // The current hue
  static uint8_t    thissat = 255; // Saturation of the colour.
  static uint8_t thisbright = 255; // How bright should the LED/display be.
  static uint8_t   basebeat =   5; // Higher = faster movement.

  static uint8_t lastSecond =  99;  // Static variable, means it's only defined once. This is our 'debounce' variable.
  uint8_t secondHand = (millis() / 1000) % 30; // IMPORTANT!!! Change '30' to a different value to change duration of the loop.

  if (lastSecond != secondHand) { // Debounce to make sure we're not repeating an assignment.
    lastSecond = secondHand;
    switch (secondHand) {
      case  0: numdots = 1; basebeat = 20; hueinc = 16; faderate = 2; thishue = 0; break; // You can change values here, one at a time , or altogether.
      case 10: numdots = 4; basebeat = 10; hueinc = 16; faderate = 8; thishue = 128; break;
      case 20: numdots = 8; basebeat =  3; hueinc =  0; faderate = 8; thishue = random8(); break; // Only gets called once, and not continuously for the next several seconds. Therefore, no rainbows.
      case 30: break;
    }
  }

  // Several colored dots, weaving in and out of sync with each other
  curhue = thishue; // Reset the hue values.
  fadeToBlackBy(leds, NUM_LEDS, faderate);
  for ( int i = 0; i < numdots; i++) {
    //beat16 is a FastLED 3.1 function
    leds[beatsin16(basebeat + i + numdots, 0, NUM_LEDS)] += CHSV(gHue + curhue, thissat, thisbright);
    curhue += hueinc;
  }

  return 8;
}

// based on FastLED example Fire2012WithPalette: https://github.com/FastLED/FastLED/blob/master/examples/Fire2012WithPalette/Fire2012WithPalette.ino
void heatMap(CRGBPalette16 palette, bool up)
{
  fill_solid(leds, NUM_LEDS, CRGB::Black);

  // Add entropy to random number generator; we use a lot of it.
  random16_add_entropy(random(256));

  // COOLING: How much does the air cool as it rises?
  // Less cooling = taller flames.  More cooling = shorter flames.
  // Default 55, suggested range 20-100
  uint8_t cooling = 55;

  // SPARKING: What chance (out of 255) is there that a new spark will be lit?
  // Higher chance = more roaring fire.  Lower chance = more flickery fire.
  // Default 120, suggested range 50-200.
  uint8_t sparking = 120;

  // Array of temperature readings at each simulation cell
  static byte heat[kMatrixWidth + 3][kMatrixHeight + 3];

  for (int x = 0; x < 10; x++)
  {
    // Step 1.  Cool down every cell a little
    for (int y = 0; y < 10; y++)
    {
      heat[x][y] = qsub8(heat[x][y], random8(0, ((cooling * 10) / kMatrixHeight) + 2));
    }

    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for (int y = 0; y < kMatrixHeight; y++)
    {
      heat[x][y] = (heat[x][y + 1] + heat[x][y + 2] + heat[x][y + 2]) / 3;
    }

    // Step 2.  Randomly ignite new 'sparks' of heat
    if (random8() < sparking)
    {
      heat[x][maxY] = qadd8(heat[x][maxY], random8(160, 255));
    }

    // Step 4.  Map from heat cells to LED colors
    for (int y = 0; y < kMatrixHeight; y++)
    {
      uint8_t colorIndex = 0;

      if (up)
        colorIndex = heat[x][y];
      else
        colorIndex = heat[x][(maxY) - y];

      // Recommend that you use values 0-240 rather than
      // the usual 0-255, as the last 15 colors will be
      // 'wrapping around' from the hot end to the cold end,
      // which looks wrong.
      colorIndex = scale8(colorIndex, 240);

      // override color 0 to ensure a black background
      if (colorIndex != 0)
      {
        setPixelXY10(x, y, ColorFromPalette(palette, colorIndex, 255, LINEARBLEND));
      }
    }
  }
}

uint8_t fire()
{
  heatMap(HeatColors_p, true);

  return 30;
}

uint8_t water()
{
  heatMap(IceColors_p, false);

  return 30;
}

uint8_t showSolidColor()
{
  fill_solid(leds, NUM_LEDS, solidColor);

  return 30;
}

uint8_t incrementalDrift() {
  uint8_t stepwidth = 256 * (20 - 1) / NUM_LEDS;
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    uint8_t bri = beatsin88(1 * 256 + (NUM_LEDS - i) * stepwidth, 0, 256);
    leds[fibonacciToPhysicalOrder[i]] = ColorFromPalette(gCurrentPalette, 2.5 * i + gHue, bri, LINEARBLEND);
  }

  return 8;
}

// Pride2015 by Mark Kriegsman: https://gist.github.com/kriegsman/964de772d64c502760e5
// This function draws rainbows with an ever-changing,
// widely-varying set of parameters.
uint8_t pride(bool useFibonacciOrder) {
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;

  uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 1, 3000);

  uint16_t ms = millis();
  uint16_t deltams = ms - sLastMillis ;
  sLastMillis  = ms;
  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88( 400, 5, 9);
  uint16_t brightnesstheta16 = sPseudotime;

  for ( uint16_t i = 0 ; i < NUM_LEDS; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    CRGB newcolor = CHSV( hue8, sat8, bri8);

    uint16_t pixelnumber = i;

    if (useFibonacciOrder) {
      pixelnumber = fibonacciToPhysicalOrder[(NUM_LEDS - 1) - pixelnumber];
    }

    nblend(leds[pixelnumber], newcolor, 64);
  }

  return 0;
}

uint8_t pride1()
{
  return pride(true);
}

uint8_t pride2()
{
  return pride(false);
}

uint8_t radialPaletteShift()
{
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    // leds[i] = ColorFromPalette( currentPalette, gHue + sin8(i*16), brightness);
    uint8_t index = fibonacciToPhysicalOrder[(NUM_LEDS - 1) - i];

    leds[index] = ColorFromPalette(gCurrentPalette, i + gHue, 255, LINEARBLEND);
  }

  return 8;
}

uint8_t horizontalPaletteBlend()
{
  uint8_t offset = 0;

  for (uint8_t x = 0; x <= kMatrixWidth; x++)
  {
    CRGB color = ColorFromPalette(currentPalette, gHue + offset, 255, LINEARBLEND);

    for (uint8_t y = 0; y <= kMatrixHeight; y++)
    {
      setPixelXY(x, y, color);
    }

    offset++;
  }

  return 15;
}

uint8_t verticalPaletteBlend()
{
  uint8_t offset = 0;

  for (uint8_t y = 0; y <= kMatrixHeight; y++)
  {
    CRGB color = ColorFromPalette(currentPalette, gHue + offset, 255, LINEARBLEND);

    for (uint8_t x = 0; x <= kMatrixWidth; x++)
    {
      setPixelXY(x, y, color);
    }

    offset++;
  }

  return 15;
}

uint8_t nyan()
{
  static uint8_t offset = 0;

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    uint8_t j = physicalToFibonacciOrder[i];
    leds[i] = ColorFromPalette(gCurrentPalette, ledRadii[j] + offset, ledAngles[j] - offset);
    // leds[i] =  CHSV(255 - ledRadii[j], 255, ledAngles[j] - offset);
  }

  EVERY_N_MILLISECONDS(15) { offset++; };

  return 8;
}

uint8_t radialWavesWithCircular()
{
  dimAll(253);

  static uint8_t prevB = 0;
  static bool spiral = false;

  uint8_t b = beatsin8(8);

  if(b < prevB){
    random16_add_entropy(random(255));
    if(random8() > 254) {
      spiral = true;
    }
  }

  prevB = b;

//  EVERY_N_SECONDS(5) { spiral = true; }

  if(spiral) {
    static uint8_t angle = random8();
    static uint8_t radius = 255;

    setPixelAR(angle, radius, 24, 20, CRGB::White);

    if(radius < 1) {
      spiral = false;
      radius = 255;
      angle = random8();
    }

    radius -= 1;
    angle -= 2;
  }

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    uint8_t j = physicalToFibonacciOrder[i];
    leds[i] |= ColorFromPalette(gCurrentPalette, ledRadii[j] - b);
  }

  return 1;
}

uint8_t circular()
{
  dimAll(253);

  static uint8_t offset = 0;

  uint8_t angle = beat8(60);
  uint8_t radius = beatsin8(16);

  setPixelAR(angle, radius, 24, 20, ColorFromPalette(gCurrentPalette, offset));
  // setPixelAR(angle, radius, 24, 20, CHSV(offset, 255, 255));

  EVERY_N_MILLISECONDS(15) { offset++; };

  return 8;
}

uint8_t radar()
{
  dimAll(253);
  static uint8_t offset = 0;

//  uint8_t a = sin8(offset);
//  uint8_t r = cos8(offset);

  setPixelA(offset, 24, CRGB::Red);

  EVERY_N_MILLISECONDS(15) { offset++; };

  return 1;
}

uint8_t angular()
{
  static uint8_t offset = 0;

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    uint8_t j = physicalToFibonacciOrder[i];
    leds[i] = ColorFromPalette(gCurrentPalette, ledAngles[j] + offset);
    // leds[i] = CHSV(ledAngles[j] + offset, 255, 255);
  }

  EVERY_N_MILLISECONDS(15) { offset++; };

  return 8;
}

uint8_t angular2()
{
  static uint8_t offset = 0;

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    uint8_t j = physicalToFibonacciOrder[i];
    leds[i] = ColorFromPalette(gCurrentPalette, ledAngles[j] + offset, ledRadii[j]);
    // leds[i] = CHSV(ledAngles[j] + offset, 255, 255 - ledRadii[j]);
  }

  EVERY_N_MILLISECONDS(15) { offset++; };

  return 8;
}

uint8_t radial()
{
  static uint8_t offset = 0;

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    uint8_t j = physicalToFibonacciOrder[i];
    leds[i] = ColorFromPalette(gCurrentPalette, ledRadii[j] + offset);
    // leds[i] = CHSV(ledRadii[j] + offset, 255, 255);
  }

  EVERY_N_MILLISECONDS(15) { offset++; };

  return 8;
}

uint8_t radial2()
{
  static uint8_t offset = 0;

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    uint8_t j = physicalToFibonacciOrder[i];
    leds[i] = ColorFromPalette(gCurrentPalette, ledRadii[j] - offset);
    // leds[i] = CHSV(ledRadii[j] + offset, ledAngles[j], 255);
  }

  EVERY_N_MILLISECONDS(15) { offset++; };

  return 8;
}

uint8_t radialWaves()
{
  static uint8_t offset = 0;

  uint8_t b = beatsin8(8);

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    uint8_t j = physicalToFibonacciOrder[i];
    leds[i] = ColorFromPalette(gCurrentPalette, ledRadii[j] - b);
    // leds[i] = CHSV(ledRadii[j] + offset, ledAngles[j], 255);
  }

  EVERY_N_MILLISECONDS(15) { offset++; };

  return 1;
}

uint8_t radial3()
{
  static uint8_t offset = 0;

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    uint8_t j = physicalToFibonacciOrder[i];
    leds[i] = ColorFromPalette(gCurrentPalette, ledRadii[j] + offset, ledAngles[j] - offset);
    // leds[i] = CHSV(ledRadii[j] + offset, 255, ledAngles[j]);
  }

  EVERY_N_MILLISECONDS(15) { offset++; };

  return 8;
}

CRGB scrollingHorizontalWashColor( uint8_t x, uint8_t y, unsigned long timeInMillis)
{
  return CHSV( x + (timeInMillis / 10), 255, 255);
}

uint8_t horizontalRainbow()
{
  unsigned long t = millis();

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    uint8_t j = physicalToFibonacciOrder[i];
    leds[i] = scrollingHorizontalWashColor(coordsX[j], coordsY[j], t);
  }

  return 8;
}

CRGB scrollingVerticalWashColor( uint8_t x, uint8_t y, unsigned long timeInMillis)
{
  return CHSV( y + (timeInMillis / 10), 255, 255);
}

uint8_t verticalRainbow()
{
  unsigned long t = millis();

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    uint8_t j = physicalToFibonacciOrder[i];
    leds[i] = scrollingVerticalWashColor(coordsX[j], coordsY[j], t);
  }

  return 8;
}

CRGB scrollingDiagonalWashColor( uint8_t x, uint8_t y, unsigned long timeInMillis)
{
  return CHSV( x + y + (timeInMillis / 10), 255, 255);
}

uint8_t diagonalRainbow()
{
  unsigned long t = millis();

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    uint8_t j = physicalToFibonacciOrder[i];
    leds[i] = scrollingDiagonalWashColor(coordsX[j], coordsY[j], t);
  }

  return 8;
}

uint8_t wave()
{
  const uint8_t scale = 256 / kMatrixWidth;

  static uint8_t rotation = 0;
  static uint8_t theta = 0;
  static uint8_t waveCount = 1;

  uint8_t n = 0;

  switch (rotation) {
    case 0:
      for (int x = 0; x < kMatrixWidth; x++) {
        n = quadwave8(x * 2 + theta) / scale;
        setPixelXY(x, n, ColorFromPalette(currentPalette, x + gHue, 255, LINEARBLEND));
        if (waveCount == 2)
          setPixelXY(x, maxY - n, ColorFromPalette(currentPalette, x + gHue, 255, LINEARBLEND));
      }
      break;

    case 1:
      for (int y = 0; y < kMatrixHeight; y++) {
        n = quadwave8(y * 2 + theta) / scale;
        setPixelXY(n, y, ColorFromPalette(currentPalette, y + gHue, 255, LINEARBLEND));
        if (waveCount == 2)
          setPixelXY(maxX - n, y, ColorFromPalette(currentPalette, y + gHue, 255, LINEARBLEND));
      }
      break;

    case 2:
      for (int x = 0; x < kMatrixWidth; x++) {
        n = quadwave8(x * 2 - theta) / scale;
        setPixelXY(x, n, ColorFromPalette(currentPalette, x + gHue));
        if (waveCount == 2)
          setPixelXY(x, maxY - n, ColorFromPalette(currentPalette, x + gHue, 255, LINEARBLEND));
      }
      break;

    case 3:
      for (int y = 0; y < kMatrixHeight; y++) {
        n = quadwave8(y * 2 - theta) / scale;
        setPixelXY(n, y, ColorFromPalette(currentPalette, y + gHue, 255, LINEARBLEND));
        if (waveCount == 2)
          setPixelXY(maxX - n, y, ColorFromPalette(currentPalette, y + gHue, 255, LINEARBLEND));
      }
      break;
  }

  dimAll(255);

  EVERY_N_SECONDS(10)
  {
    rotation = random(0, 4);
    // waveCount = random(1, 3);
  };

  EVERY_N_MILLISECONDS(7) {
    theta++;
  }

  return 8;
}

uint8_t pulse()
{
  dimAll(200);

  uint8_t maxSteps = 16;
  static uint8_t step = maxSteps;
  static uint8_t centerX = 0;
  static uint8_t centerY = 0;
  float fadeRate = 0.8;

  if (step >= maxSteps)
  {
    centerX = random(kMatrixWidth);
    centerY = random(kMatrixWidth);
    step = 0;
  }

  if (step == 0)
  {
    drawCircle(centerX, centerY, step, ColorFromPalette(currentPalette, gHue, 255, LINEARBLEND));
    step++;
  }
  else
  {
    if (step < maxSteps)
    {
      // initial pulse
      drawCircle(centerX, centerY, step, ColorFromPalette(currentPalette, gHue, pow(fadeRate, step - 2) * 255, LINEARBLEND));

      // secondary pulse
      if (step > 3) {
        drawCircle(centerX, centerY, step - 3, ColorFromPalette(currentPalette, gHue, pow(fadeRate, step - 2) * 255, LINEARBLEND));
      }

      step++;
    }
    else
    {
      step = -1;
    }
  }

  return 30;
}

// ColorWavesWithPalettes by Mark Kriegsman: https://gist.github.com/kriegsman/8281905786e8b2632aeb
// This function draws color waves with an ever-changing,
// widely-varying set of parameters, using a color palette.
void colorwaves( CRGB* ledarray, uint16_t numleds, CRGBPalette16& palette, bool useFibonacciOrder)
{
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;

  // uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 300, 1500);

  uint16_t ms = millis();
  uint16_t deltams = ms - sLastMillis ;
  sLastMillis  = ms;
  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88( 400, 5, 9);
  uint16_t brightnesstheta16 = sPseudotime;

  for ( uint16_t i = 0 ; i < numleds; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;
    uint16_t h16_128 = hue16 >> 7;
    if ( h16_128 & 0x100) {
      hue8 = 255 - (h16_128 >> 1);
    } else {
      hue8 = h16_128 >> 1;
    }

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    uint8_t index = hue8;
    //index = triwave8( index);
    index = scale8( index, 240);

    CRGB newcolor = ColorFromPalette( palette, index, bri8);

    uint16_t pixelnumber = i;

    if (useFibonacciOrder) {
      pixelnumber = fibonacciToPhysicalOrder[(numleds - 1) - pixelnumber];
    }

    nblend( ledarray[pixelnumber], newcolor, 128);
  }
}

uint8_t colorWaves1()
{
  colorwaves( leds, NUM_LEDS, gCurrentPalette, true);
  return 0;
}

uint8_t colorWaves2()
{
  colorwaves( leds, NUM_LEDS, gCurrentPalette, false);
  return 0;
}

// Alternate rendering function just scrolls the current palette
// across the defined LED strip.
void palettetest( CRGB* ledarray, uint16_t numleds, const CRGBPalette16& gCurrentPalette)
{
  static uint8_t startindex = 0;
  startindex--;
  fill_palette( ledarray, numleds, startindex, (256 / NUM_LEDS) + 1, gCurrentPalette, 255, LINEARBLEND);
}

typedef uint8_t (*SimplePattern)();
typedef SimplePattern SimplePatternList[];
typedef struct { SimplePattern drawFrame;  String name; } PatternAndName;
typedef PatternAndName PatternAndNameList[];

// List of patterns to cycle through.  Each is defined as a separate function below.

#include "Life.h"
#include "Twinkles.h"
#include "Noise.h"

const PatternAndNameList patterns = {
  // Fibonacci patterns
  { pride1,                  "Pride1" },
  { pride2,                  "Pride2" },
  { colorWaves1,             "ColorWaves1" },
  { colorWaves2,             "ColorWaves2" },
  { incrementalDrift,        "IncrementalDrift" },

  // palette shift/blends
  { radialPaletteShift,      "radialPaletteShift" },
  { verticalPaletteBlend,    "verticalPaletteBlend" },
  { horizontalPaletteBlend,  "horizontalPaletteBlend" },
  { horizontalRainbow,       "horizontalRainbow" },
  { verticalRainbow,         "verticalRainbow" },
  { diagonalRainbow,         "diagonalRainbow" },

  // noise patterns
  { fireNoise,               "fireNoise" },
  { fireNoise2,              "fireNoise2" },
  { lavaNoise,               "lavaNoise" },
  { rainbowNoise,            "rainbowNoise" },
  { rainbowStripeNoise,      "rainbowStripeNoise" },
  { partyNoise,              "partyNoise" },
  { forestNoise,             "forestNoise" },
  { cloudNoise,              "cloudNoise" },
  { oceanNoise,              "oceanNoise" },
  { blackAndWhiteNoise,      "blackAndWhiteNoise" },
  { blackAndBlueNoise,       "blackAndBlueNoise" },

  // radial patterns
  { radialWavesWithCircular, "radialWavesWithCircular" },
  { radialWaves,             "radialWaves" },
  { radial,                  "radial" },
  { radial2,                 "radial2" },
  { radial3,                 "radial3" },
  { nyan,                    "nyan" },
  { circular,                "circular" },
  { angular,                 "angular" },
  { angular2,                "angular2" },

  // 2D XY patterns
  { wave,                    "wave" },
  { life,                    "life" },
  { pulse,                   "pulse" },
  { fire,                    "fire" },
  { water,                   "water" },

  // 1D patterns
  { rainbow,                 "rainbow" },
  { rainbowWithGlitter,      "rainbowWithGlitter" },
  { rainbowSolid,            "rainbowSolid" },
  { sinelon1,                "sinelon1" },
  { sinelon2,                "sinelon2" },
  { bpm1,                    "bpm1" },
  { bpm2,                    "bpm2" },
  { juggle,                  "juggle" },
  { juggle2,                 "juggle2" },
  { confetti,                "confetti" },
  { rainbowTwinkles,         "rainbowTwinkles" },
  { snowTwinkles,            "snowTwinkles" },
  { cloudTwinkles,           "cloudTwinkles" },
  { incandescentTwinkles,    "incandescentTwinkles" },

  { showSolidColor,          "showSolidColor" }
};

int patternCount = ARRAY_SIZE(patterns);

void loadSettings() {
  // load settings from EEPROM

  // brightness
  brightness = EEPROM.read(0);
  if (brightness < 1)
    brightness = 1;
  else if (brightness > 255)
    brightness = 255;

  // patternIndex
  patternIndex = EEPROM.read(1);
  if (patternIndex < 0)
    patternIndex = 0;
  else if (patternIndex >= patternCount)
    patternIndex = patternCount - 1;

  // solidColor
  solidColor.r = EEPROM.read(2);
  solidColor.g = EEPROM.read(3);
  solidColor.b = EEPROM.read(4);

  if (solidColor.r == 0 && solidColor.g == 0 && solidColor.b == 0)
    solidColor = CRGB::White;
}

void setSolidColor(CRGB color) {
  solidColor = color;

  EEPROM.write(2, solidColor.r);
  EEPROM.write(3, solidColor.g);
  EEPROM.write(4, solidColor.b);

  setPattern(patternCount - 1);
}

uint8_t variableCursor = 0;

int moveVariableCursor(String args)
{
    if(args.startsWith("pwr")) {
        itoa(power, variableValue, 10);
        return power;
    }
    else if (args.startsWith("brt")) {
        itoa(brightness, variableValue, 10);
        return brightness;
    }
    else if (args.startsWith("r")) {
        itoa(r, variableValue, 10);
        return r;
    }
    else if (args.startsWith("g")) {
        itoa(g, variableValue, 10);
        return g;
    }
    else if (args.startsWith("b")) {
        itoa(b, variableValue, 10);
        return b;
    }

    return 0;
}

int setVariable(String args) {
    if(args.startsWith("pwr:")) {
        return setPower(args.substring(4));
    }
    else if (args.startsWith("brt:")) {
        return setBrightness(args.substring(4));
    }
    else if (args.startsWith("r:")) {
        r = parseByte(args.substring(2));
        solidColor.r = r;
        EEPROM.write(2, r);
        patternIndex = patternCount - 1;
        return r;
    }
    else if (args.startsWith("g:")) {
        g = parseByte(args.substring(2));
        solidColor.g = g;
        EEPROM.write(3, g);
        patternIndex = patternCount - 1;
        return g;
    }
    else if (args.startsWith("b:")) {
        b = parseByte(args.substring(2));
        solidColor.b = b;
        EEPROM.write(4, b);
        patternIndex = patternCount - 1;
        return b;
    }

    return -1;
}

int setPower(String args) {
    power = args.toInt();
    if(power < 0)
        power = 0;
    else if (power > 1)
        power = 1;

    return power;
}

int setBrightness(String args)
{
    brightness = args.toInt();
    if(brightness < 1)
        brightness = 1;
    else if(brightness > 255)
        brightness = 255;

    FastLED.setBrightness(brightness);
    /*FastLED.setDither(brightness < 255);*/

    EEPROM.write(0, brightness);

    return brightness;
}

byte parseByte(String args) {
    int c = args.toInt();
    if(c < 0)
        c = 0;
    else if (c > 255)
        c = 255;

    return c;
}

void setPattern(int value)
{
  // don't wrap around at the ends
  if (value < 0)
    value = 0;
  else if (value >= patternCount)
    value = patternCount - 1;

  patternIndex = value;

  EEPROM.write(1, patternIndex);
}

int setPatternIndex(String args)
{
  int value = args.toInt();
  setPattern(value);
  return patternIndex;
}

int movePatternNameCursor(String args)
{
    int index = args.toInt();
    if(index < 0)
        index = 0;
    else if (index >= patternCount)
        index = patternCount - 1;

    patternName = patterns[index].name;

    return index;
}

void setup()
{
  Serial.begin(9600);

  FastLED.addLeds<LED_TYPE, DATA_PIN>(leds, NUM_LEDS);
  FastLED.setCorrection(Typical8mmPixel);
  FastLED.setBrightness(brightness);
  FastLED.setDither(false);
  fill_solid(leds, NUM_LEDS, solidColor);
  FastLED.show();

  // Initialize the IR receiver
  irReceiver.enableIRIn();
  // irReceiver.blink13(false);

  /*pinMode(BUTTON_1_PIN, INPUT_PULLUP);
  button1.attach(BUTTON_1_PIN);
  button1.interval(5);

  pinMode(BUTTON_2_PIN, INPUT_PULLUP);
  button2.attach(BUTTON_2_PIN);
  button2.interval(5);*/

  // loadSettings();

  FastLED.setBrightness(brightness);
  /*FastLED.setDither(brightness < 255);*/

  noisex = random16();
  noisey = random16();
  noisez = random16();

  Particle.function("patternIndex", setPatternIndex); // sets the current pattern index, changes to the pattern with the specified index
  Particle.function("pNameCursor", movePatternNameCursor); // moves the pattern name cursor to the specified index, allows getting a list of pattern names
  Particle.function("variable", setVariable); // sets the value of a variable, args are name:value
  Particle.function("varCursor", moveVariableCursor);

  Particle.variable("power", power);
  Particle.variable("brightness", brightness);
  Particle.variable("patternCount", patternCount);
  Particle.variable("patternIndex", patternIndex);
  Particle.variable("patternName", patternName);
  Particle.variable("variable", variableValue);
}

void loop()
{
  /*handleIrInput();*/

  if(power < 1) {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    FastLED.delay(15);
    return;
  }

  // Add entropy to random number generator; we use a lot of it.
  random16_add_entropy(random(255));

  uint8_t requestedDelay = patterns[patternIndex].drawFrame();

  // send the 'leds' array out to the actual LED strip
  FastLED.show();
  FastLED.delay(requestedDelay);

  if (autoplayEnabled && millis() > autoPlayTimout) {
    adjustPattern(true);
    autoPlayTimout = millis() + (autoPlayDurationSeconds * 1000);
  }

  // blend the current palette to the next
  EVERY_N_MILLISECONDS(40) {
    nblendPaletteTowardPalette(currentPalette, targetPalette, 16);
  }

  EVERY_N_MILLISECONDS( 40 ) {
    gHue++;  // slowly cycle the "base color" through the rainbow
  }

  // slowly change to a new palette
  EVERY_N_SECONDS(SECONDS_PER_PALETTE) {
    paletteIndex++;
    if (paletteIndex >= paletteCount) paletteIndex = 0;
    targetPalette = palettes[paletteIndex];
  };

  // slowly change to a new cpt-city gradient palette
  EVERY_N_SECONDS( SECONDS_PER_PALETTE ) {
    gCurrentPaletteNumber = addmod8( gCurrentPaletteNumber, 1, gGradientPaletteCount);
    gTargetPalette = gGradientPalettes[ gCurrentPaletteNumber ];
  }

  // blend the current cpt-city gradient palette to the next
  EVERY_N_MILLISECONDS(40) {
    nblendPaletteTowardPalette( gCurrentPalette, gTargetPalette, 16);
  }
}

void handleIrInput()
{
  InputCommand command = readCommand(defaultHoldDelay);

  if (command != InputCommand::None) {
    Serial.print("command: ");
    Serial.println((int) command);
  }

  switch (command) {
    case InputCommand::Up: {
        adjustPattern(true);
        break;
      }
    case InputCommand::Down: {
        adjustPattern(false);
        break;
      }
    case InputCommand::Power: {
        power = power == 0 ? 1 : 0;
        break;
      }
    case InputCommand::BrightnessUp: {
        adjustBrightness(true);
        break;
      }
    case InputCommand::BrightnessDown: {
        adjustBrightness(false);
        break;
      }

    // pattern buttons

    case InputCommand::Pattern1: {
        setPattern(0);
        break;
      }
    case InputCommand::Pattern2: {
        setPattern(1);
        break;
      }
    case InputCommand::Pattern3: {
        setPattern(2);
        break;
      }
    case InputCommand::Pattern4: {
        setPattern(3);
        break;
      }
    case InputCommand::Pattern5: {
        setPattern(4);
        break;
      }
    case InputCommand::Pattern6: {
        setPattern(5);
        break;
      }
    case InputCommand::Pattern7: {
        setPattern(6);
        break;
      }
    case InputCommand::Pattern8: {
        setPattern(7);
        break;
      }
    case InputCommand::Pattern9: {
        setPattern(8);
        break;
      }
    case InputCommand::Pattern10: {
        setPattern(9);
        break;
      }
    case InputCommand::Pattern11: {
        setPattern(10);
        break;
      }
    case InputCommand::Pattern12: {
        setPattern(11);
        break;
      }

    // color buttons

    case InputCommand::Red: {
        setSolidColor(CRGB::Red);
        break;
      }
    case InputCommand::RedOrange: {
        setSolidColor(CRGB::OrangeRed);
        break;
      }
    case InputCommand::Orange: {
        setSolidColor(CRGB::Orange);
        break;
      }
    case InputCommand::YellowOrange: {
        setSolidColor(CRGB::Goldenrod);
        break;
      }
    case InputCommand::Yellow: {
        setSolidColor(CRGB::Yellow);
        break;
      }

    case InputCommand::Green: {
        setSolidColor(CRGB::Green);
        break;
      }
    case InputCommand::Lime: {
        setSolidColor(CRGB::Lime);
        break;
      }
    case InputCommand::Aqua: {
        setSolidColor(CRGB::Aqua);
        break;
      }
    case InputCommand::Teal: {
        setSolidColor(CRGB::Teal);
        break;
      }
    case InputCommand::Navy: {
        setSolidColor(CRGB::Navy);
        break;
      }

    case InputCommand::Blue: {
        setSolidColor(CRGB::Blue);
        break;
      }
    case InputCommand::RoyalBlue: {
        setSolidColor(CRGB::RoyalBlue);
        break;
      }
    case InputCommand::Purple: {
        setSolidColor(CRGB::Purple);
        break;
      }
    case InputCommand::Indigo: {
        setSolidColor(CRGB::Indigo);
        break;
      }
    case InputCommand::Magenta: {
        setSolidColor(CRGB::Magenta);
        break;
      }

    case InputCommand::White: {
        setSolidColor(CRGB::White);
        break;
      }
    case InputCommand::Pink: {
        setSolidColor(CRGB::Pink);
        break;
      }
    case InputCommand::LightPink: {
        setSolidColor(CRGB::LightPink);
        break;
      }
    case InputCommand::BabyBlue: {
        setSolidColor(CRGB::CornflowerBlue);
        break;
      }
    case InputCommand::LightBlue: {
        setSolidColor(CRGB::LightBlue);
        break;
      }
  }
}

// increase or decrease the current pattern number, and wrap around at the ends
void adjustPattern(bool up)
{
  if (up)
    patternIndex++;
  else
    patternIndex--;

  // wrap around at the ends
  if (patternIndex < 0)
    patternIndex = patternCount - 1;
  if (patternIndex >= patternCount)
    patternIndex = 0;

  EEPROM.write(1, patternIndex);
}
