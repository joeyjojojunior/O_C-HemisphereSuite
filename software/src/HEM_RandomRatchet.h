// Copyright (c) 2018, Jason Justian
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "util/util_misc.h"

#define HEM_RR_SPACING_MIN 8
#define HEM_RR_DIV_MAX 16
#define HEM_RR_PROB_MAX 100
#define HEM_RR_DIST_MAX 2

#define HEM_RR_TUPLETS_MAX 2
#define HEM_RR_TUPLETS_NONE 0
#define HEM_RR_TUPLETS_DUPLETS 1
#define HEM_RR_TUPLETS_TRIPLETS 2

#define HEM_RR_NUMBER_MAX 48
#define HEM_RR_NUMBER_INDEX_MAX 32
#define HEM_RR_NUMBER_INDEX_MAX_DUPLETS 5
#define HEM_RR_NUMbER_INDEX_MAX_TRIPLETS 5
#define HEM_RR_NUMBER_MIN 1

#define HEM_RR_COL0_X 1
#define HEM_RR_COL1_X 22
#define HEM_RR_COL2_X 44
#define HEM_RR_ROW0_Y 15
#define HEM_RR_ROW_INTERVAL 12

#define HEM_RR_NUM_MENU_ITEMS 12
#define HEM_RR_PARAM_PROB1 0
#define HEM_RR_PARAM_NUM1 1
#define HEM_RR_PARAM_DIV1 2
#define HEM_RR_PARAM_DIST1 3
#define HEM_RR_PARAM_TUPLETS1 4
#define HEM_RR_PARAM_SETTINGS1 5
#define HEM_RR_PARAM_PROB2 6
#define HEM_RR_PARAM_NUM2 7
#define HEM_RR_PARAM_DIV2 8
#define HEM_RR_PARAM_DIST2 9
#define HEM_RR_PARAM_TUPLETS2 10
#define HEM_RR_PARAM_SETTINGS2 11

#define HEM_RR_MOD_MAX 102
#define HEM_RR_MOD_OFFSET_PROB 2
#define HEM_RR_NUMBER_OF_MODS 5
#define HEM_RR_PARAM_MOD_PROB 0
#define HEM_RR_PARAM_MOD_NUM 1
#define HEM_RR_PARAM_MOD_DIV 2
#define HEM_RR_PARAM_MOD_DIST 3
#define HEM_RR_PARAM_MOD_TUPLETS 4

class RandomRatchet : public HemisphereApplet {
public:
    const char* applet_name() {
        return "RandRatch";
    }

    void Start() {
        cursor = 0;
        last_number_cv_tick = 0;
        edit_param = -1;
    }

    void Controller() {
        last_number_cv_tick = OC::CORE::ticks;
        processCV();

        // Get timing information
        if (Clock(0)) {
            // Get a tempo, if this is the second tick or later since the last clock
            ch1.spacing = (ticks_since_clock / ch1.number) / 17;
            ch2.spacing = (ticks_since_clock / ch2.number) / 17;
            ticks_since_clock = 0;
        }

        if (Clock(1)) processRand();

        ticks_since_clock++;

        // Get spacing with clock division calculated
        getEffectiveSpacing(&ch1);
        getEffectiveSpacing(&ch2);

        // Handle a burst set in progress
        handleBurst(&ch1, 0);
        handleBurst(&ch2, 1);

        // Handle the triggering of a new burst set.
        //
        // number_is_changing: If Number is being changed via CV, employ the ADC Lag mechanism
        // so that Number can be set and gated with a sequencer (or something). Otherwise, if
        // Number is not being changed via CV, fire the set of bursts right away. This is done so that
        // the applet can adapt to contexts that involve (1) the need to accurately interpret rapidly-
        // changing CV values or (2) the need for tight timing when Number is static-ish.
        bool number_is_changing = (OC::CORE::ticks - last_number_cv_tick < 80000);
        if (Clock(0) && number_is_changing) StartADCLag();

        if (EndOfADCLag() || (Clock(0) && !number_is_changing)) {
            for (int i = 0; i < 2; i++) {
                channel *ch = channels[i];
                bool should_fire_burst = random() % 100 < ch->prob;
                if (should_fire_burst) {
                    ClockOut(i);
                    ch->bursts_to_go = ch->number - 1;
                    ch->burst_countdown = ch->effective_spacing * 17;
                }   
            }
        }
    }

    void View() {
        gfxHeader(applet_name());
        DrawSelector();
    }

    void OnButtonPress() {
        if (cursor == HEM_RR_PARAM_SETTINGS1) {
            ch1.settings = !ch1.settings;
        } else if (cursor == HEM_RR_PARAM_SETTINGS2) {
            ch2.settings = !ch2.settings;
        } else {
            edit_param = (edit_param == cursor) ? -1 : cursor;
        }
    }

    void OnEncoderMove(int direction) {
        channel *ch = (edit_param < HEM_RR_PARAM_SETTINGS1) ? &ch1 : &ch2;

        if (!ch->settings) {
            switch (edit_param) {
                case HEM_RR_PARAM_PROB1:
                case HEM_RR_PARAM_PROB2:
                    ch->prob = constrain(ch->prob += direction, 0, HEM_RR_PROB_MAX);
                break;
                case HEM_RR_PARAM_NUM1:
                case HEM_RR_PARAM_NUM2:
                    ch->number_index = constrain(ch->number_index += direction, 0, getMaxTuplets(ch));
                    updateNumbers(ch);
                break;
                case HEM_RR_PARAM_DIV1:
                case HEM_RR_PARAM_DIV2:
                    ch->div = constrain(ch->div += direction, 1, HEM_RR_DIV_MAX);
                break;
                case HEM_RR_PARAM_DIST1:
                case HEM_RR_PARAM_DIST2:
                    ch->dist = constrain(ch->dist += direction, 0, HEM_RR_DIST_MAX);
                break;
                case HEM_RR_PARAM_TUPLETS1:
                case HEM_RR_PARAM_TUPLETS2:
                    ch->tuplets = constrain(ch->tuplets += direction, 0, HEM_RR_TUPLETS_MAX);
                    updateNumbers(ch);
                break;
            }
        } else {
            switch (edit_param) {
                case HEM_RR_PARAM_PROB1:
                case HEM_RR_PARAM_PROB2:
                    ch->mod[HEM_RR_PARAM_MOD_PROB] = constrain(modulo(ch->mod[HEM_RR_PARAM_MOD_PROB] += direction, HEM_RR_MOD_MAX + 1), 0, HEM_RR_MOD_MAX);
                break;
                case HEM_RR_PARAM_NUM1:
                case HEM_RR_PARAM_NUM2:
                    ch->mod[HEM_RR_PARAM_MOD_NUM] = constrain(modulo(ch->mod[HEM_RR_PARAM_MOD_NUM] += direction, HEM_RR_MOD_MAX + 1), 0, HEM_RR_MOD_MAX);
                break;
                case HEM_RR_PARAM_DIV1:
                case HEM_RR_PARAM_DIV2:
                    ch->mod[HEM_RR_PARAM_MOD_DIV] = constrain(modulo(ch->mod[HEM_RR_PARAM_MOD_DIV] += direction, HEM_RR_MOD_MAX + 1), 0, HEM_RR_MOD_MAX);
                break;
                case HEM_RR_PARAM_DIST1:
                case HEM_RR_PARAM_DIST2:
                    ch->mod[HEM_RR_PARAM_MOD_DIST] = constrain(modulo(ch->mod[HEM_RR_PARAM_MOD_DIST] += direction, HEM_RR_MOD_MAX + 1), 0, HEM_RR_MOD_MAX);
                break;
                case HEM_RR_PARAM_TUPLETS1:
                case HEM_RR_PARAM_TUPLETS2:
                    ch->mod[HEM_RR_PARAM_MOD_TUPLETS] = constrain(modulo(ch->mod[HEM_RR_PARAM_MOD_TUPLETS] += direction, HEM_RR_MOD_MAX + 1), 0, HEM_RR_MOD_MAX);
                break;
            }     
        }

        if (edit_param < 0) cursor = modulo(cursor + direction, HEM_RR_NUM_MENU_ITEMS);
    }
        
    uint32_t OnDataRequest() {
        uint32_t data = 0;

        for (int i = 0; i < 2; i++) {
            channel *ch = channels[i];
            int loc_incr = (i == 0) ? 0 : 16;

            Pack(data, PackLocation {0 + loc_incr, 8}, ch->number);
            Pack(data, PackLocation {8 + loc_incr, 4}, ch->div);
            Pack(data, PackLocation {12 + loc_incr, 2}, ch->dist);
            Pack(data, PackLocation {14 + loc_incr, 2}, ch->tuplets);
        }
        return data;
    }

    void OnDataReceive(uint32_t data) {
        for (int i = 0; i < 2; i++) {
            channel *ch = channels[i];
            int loc_incr = (i == 0) ? 0 : 16;

            ch->number = Unpack(data, PackLocation {0 + loc_incr, 8});
            ch->div = Unpack(data, PackLocation {8 + loc_incr, 4});
            ch->dist = Unpack(data, PackLocation {12 + loc_incr, 2});
            ch->tuplets = Unpack(data, PackLocation {14 + loc_incr, 2});
            updateNumbers(ch);
        }
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Clock 2=Rand Clk";
        help[HEMISPHERE_HELP_CVS]      = "Freely Assignable";
        help[HEMISPHERE_HELP_OUTS]     = "1=Trig1 2=Trig2";
        help[HEMISPHERE_HELP_ENCODER]  = "Push to Edit Param";
        //                               "------------------" <-- Size Guide
    }
    
private:
    // Settings
    int cursor;                 // Cursor position
    int edit_param;             // Current parameter selected for editing
    int ticks_since_clock;      // Time since the last clock.
    int last_number_cv_tick;    // The last time the number was changed via CV. This is used to
                                // decide whether the ADC delay should be used when clocks come in.                       
    typedef struct {
        int spacing;            // Spacing between triggers
        int effective_spacing;  // Effective spacing after division/distribution
        int burst_countdown;    // Counts down # of ticks until next trigger in ratchet
        int bursts_to_go;       // How many triggers to go for current ratchet
        int prob;               // Probability we get a ratchet on a clock pulse
        int number;             // Number of triggers per ratchet
        int number_index;       // Index to number of triggers per ratchet; used to determine doubles/triplets
        int div;                // Divides the spacing between ratchets in divisions of the clock
        int dist;               // Distribution of ratchets (increasing/decreasing space for each subsequent ratchet)
        int tuplets;            // Sets whether numbers are restricted to doubles, triplets, or none
        bool settings;          // Flag for mod settings page
        int mod[5];             // Modulation assignment for prob/number_index/div/dist/tuplets
    } channel;

    channel ch1 = { 50, 50, 0, 0, 50, 1, 0, 1, 0, 0, false, {0} };
    channel ch2 = { 50, 50, 0, 0, 50, 1, 0, 1, 0, 0, false, {0} };
    channel *channels[2] = { &ch1, &ch2 };

    // Labels for menu items
    const char *tuplet_labels[4] = {"*", "D", "T", "B"};
    const char *dist_labels[3] = {"*", "-", "+"};

    // Co-ordinates for menu items
    int menu_coords[HEM_RR_NUM_MENU_ITEMS][2] = {
        {HEM_RR_COL0_X, HEM_RR_ROW0_Y},                              // prob1/prob1 mod
        {HEM_RR_COL1_X, HEM_RR_ROW0_Y},                              // num1/num1 mod
        {HEM_RR_COL2_X, HEM_RR_ROW0_Y},                              // div1/div1 mod
        {HEM_RR_COL0_X, HEM_RR_ROW0_Y + HEM_RR_ROW_INTERVAL},        // tuplets1/tuplets1 mod
        {HEM_RR_COL1_X, HEM_RR_ROW0_Y + HEM_RR_ROW_INTERVAL},        // accel1/accel1 mod
        {HEM_RR_COL2_X, HEM_RR_ROW0_Y + HEM_RR_ROW_INTERVAL},        // settings1
        {HEM_RR_COL0_X, HEM_RR_ROW0_Y + 2*HEM_RR_ROW_INTERVAL},      // prob2/prob2 mod
        {HEM_RR_COL1_X, HEM_RR_ROW0_Y + 2*HEM_RR_ROW_INTERVAL},      // num2/num2 mod
        {HEM_RR_COL2_X, HEM_RR_ROW0_Y + 2*HEM_RR_ROW_INTERVAL},      // div2/div2 mod
        {HEM_RR_COL0_X, HEM_RR_ROW0_Y + 3*HEM_RR_ROW_INTERVAL},      // tuplets1/tuplets1 mod
        {HEM_RR_COL1_X, HEM_RR_ROW0_Y + 3*HEM_RR_ROW_INTERVAL},      // accel1/accel1 mod
        {HEM_RR_COL2_X, HEM_RR_ROW0_Y + 3*HEM_RR_ROW_INTERVAL},      // settings
    };

    void DrawSelector() {
        for (int i = 0; i < 2; i++) {
            channel *ch = channels[i];
            int y1 = HEM_RR_ROW0_Y + i*HEM_RR_ROW_INTERVAL*2;
            int y2 = HEM_RR_ROW0_Y + HEM_RR_ROW_INTERVAL + i*HEM_RR_ROW_INTERVAL*2;

            if (!ch->settings) {
                gfxPrint(HEM_RR_COL0_X, y1, ch->prob);
                gfxPrint(HEM_RR_COL1_X, y1, ch->number);
                gfxPrint(HEM_RR_COL2_X, y1, "/");
                gfxPrint(ch->div);

                gfxPrint(HEM_RR_COL0_X, y2, dist_labels[ch->dist]);
                gfxPrint(HEM_RR_COL1_X, y2, tuplet_labels[ch->tuplets]);
                gfxPrint(HEM_RR_COL2_X, y2, "S");
            } else {
                for (int j = 0; j < HEM_RR_NUMBER_OF_MODS; j++) {
                    int mod_y = (j < 3) ? y1 : y2;
                    int mod_x = HEM_RR_COL0_X;

                    if (j == 1 || j == 4) mod_x = HEM_RR_COL1_X;
                    if (j == 2) mod_x = HEM_RR_COL2_X;

                    switch (ch->mod[j]) {
                        case 0:
                            gfxPrint(mod_x, mod_y, "*");
                        break;
                        case 1:
                            gfxPrint(mod_x, mod_y, "cv1");
                        break;
                        case 2:
                            gfxPrint(mod_x, mod_y, "cv2");
                        break;
                        default:
                            gfxPrint(mod_x, mod_y, ch->mod[j] - HEM_RR_MOD_OFFSET_PROB);
                        break;                
                    }
                }
                gfxPrint(HEM_RR_COL2_X, y2, "<");
            }
        }

        if (edit_param < 0) {
            gfxCursorSolid(menu_coords[cursor][0] + 1, 10 + menu_coords[cursor][1], 12);
        } else {
            gfxCursor(menu_coords[cursor][0] + 1, 10 + menu_coords[cursor][1], 12);
        }
    }

    void processCV() {
        for (int i = 0; i < 2; i++) {
            channel *ch = channels[i];
            for (int cvIn = 0; cvIn < 2; cvIn++) {
                if (ch->mod[HEM_RR_PARAM_MOD_PROB] == cvIn + 1) {
                    getCV(cvIn, &(ch->prob), 0, HEM_RR_PROB_MAX);
                }       
                if (ch->mod[HEM_RR_PARAM_MOD_NUM] == cvIn + 1) {
                    getCV(cvIn, &(ch->number_index), 0, getMaxTuplets(ch));
                    updateNumbers(ch);
                }  
                if (ch->mod[HEM_RR_PARAM_MOD_DIV] == cvIn + 1) {
                    getCV(cvIn, &(ch->div), 1, HEM_RR_DIV_MAX);
                }  
                if (ch->mod[HEM_RR_PARAM_MOD_DIST] == cvIn + 1) {
                    getCV(cvIn, &(ch->dist), 0, HEM_RR_DIST_MAX);
                }  
                if (ch->mod[HEM_RR_PARAM_MOD_TUPLETS] == cvIn + 1) {
                    getCV(cvIn, &(ch->tuplets), 0, HEM_RR_TUPLETS_MAX);
                    updateNumbers(ch);
                }  
            }
        }
    }

    void processRand() {
         for (int i = 0; i < 2; i++) {
            channel *ch = channels[i];
            int rand = 0;
            if (ch->mod[HEM_RR_PARAM_MOD_PROB] > 2) {
                ch->prob = random(0, HEM_RR_PROB_MAX + 1) * (ch->mod[HEM_RR_PARAM_MOD_PROB] - HEM_RR_MOD_OFFSET_PROB)/100;
            }       
            if (ch->mod[HEM_RR_PARAM_MOD_NUM] > 2) {
                rand = random(0, getMaxTuplets(ch) + 1) * (ch->mod[HEM_RR_PARAM_MOD_NUM] - HEM_RR_MOD_OFFSET_PROB)/100;   
                ch->number_index = rand;  
                updateNumbers(ch);        
            }  
            if (ch->mod[HEM_RR_PARAM_MOD_DIV] > 2) {
                rand = random(1, HEM_RR_DIV_MAX + 1) * (ch->mod[HEM_RR_PARAM_MOD_DIV] - HEM_RR_MOD_OFFSET_PROB)/100;
                ch->div = constrain(rand, 1, HEM_RR_DIV_MAX);              
            }  
            if (ch->mod[HEM_RR_PARAM_MOD_DIST] > 2) {
                ch->dist = random(0, HEM_RR_DIST_MAX + 1) * (ch->mod[HEM_RR_PARAM_MOD_DIST] - HEM_RR_MOD_OFFSET_PROB)/100;                
            }  
            if (ch->mod[HEM_RR_PARAM_MOD_TUPLETS] > 2) {
                ch->tuplets = random(0, HEM_RR_TUPLETS_MAX + 1) * (ch->mod[HEM_RR_PARAM_MOD_TUPLETS] - HEM_RR_MOD_OFFSET_PROB)/100;           
                updateNumbers(ch);
            }      
        }
    }

    void getCV(int input, int *var, int min, int max) {
        *var = ProportionCV(In(input), max);
        *var = constrain(*var, min, max);
    }

    void getEffectiveSpacing(channel *ch) {
        ch->effective_spacing = ch->spacing * ch->div;
        if (ch->dist > 0) {
            ch->effective_spacing = ch->effective_spacing + (ch->dist * ch->bursts_to_go * (ch->spacing / ch->number));
        }
    }

    void handleBurst(channel *ch, int clockOut) {
        if (ch->bursts_to_go > 0) {
            if (--(ch->burst_countdown) <= 0) {
                if (ch->effective_spacing < HEM_RR_SPACING_MIN) ch->effective_spacing = HEM_RR_SPACING_MIN;
                ClockOut(clockOut);
                if (--(ch->bursts_to_go) > 0) ch->burst_countdown = ch->effective_spacing * 17; // Reset for next burst
            }
        }
    }

    int getMaxTuplets(channel *ch) {
        int max_tuplets = 0;

        switch (ch->tuplets) {
            case HEM_RR_TUPLETS_NONE:
                return HEM_RR_NUMBER_INDEX_MAX;
            break;
            case HEM_RR_TUPLETS_DUPLETS:
                return HEM_RR_NUMBER_INDEX_MAX_DUPLETS;
            break;
            case HEM_RR_TUPLETS_TRIPLETS:
                return HEM_RR_NUMbER_INDEX_MAX_TRIPLETS;
            break;
        }
        return max_tuplets;
    }

    void updateNumbers(channel *ch) {
        switch (ch->tuplets) {
            case HEM_RR_TUPLETS_NONE:
                ch->number = constrain(ch->number_index, HEM_RR_NUMBER_MIN, HEM_RR_NUMBER_INDEX_MAX);
            break;
            case HEM_RR_TUPLETS_DUPLETS:
                ch->number = constrain(power(2, ch->number_index), HEM_RR_NUMBER_MIN, HEM_RR_NUMBER_INDEX_MAX);
            break;
            case HEM_RR_TUPLETS_TRIPLETS:
                ch->number = constrain(power(2, ch->number_index) + power(2, ch->number_index - 1), HEM_RR_NUMBER_MIN, HEM_RR_NUMBER_MAX);
            break;
        }
    }

    void gfxCursorSolid(int x, int y, int w) {
        gfxLine(x, y, x + w - 1, y);
    }

    int modulo(int a, int b) {
        const int result = a % b;
        return result >= 0 ? result : result + b;
    }

    int power(int base, int exp) {
        if (exp < 0) return 0;

        int i, result = 1;
        for (i = 0; i < exp; i++) {
            result *= base;
        }
        return result;
    }
};


////////////////////////////////////////////////////////////////////////////////
//// Hemisphere Applet Functions
///
///  Once you run the find-and-replace to make these refer to Burst,
///  it's usually not necessary to do anything with these functions. You
///  should prefer to handle things in the HemisphereApplet child class
///  above.
////////////////////////////////////////////////////////////////////////////////
RandomRatchet RandomRatchet_Instance[2];

void RandomRatchet_Start(bool hemisphere) {
    RandomRatchet_Instance[hemisphere].BaseStart(hemisphere);
}

void RandomRatchet_Controller(bool hemisphere, bool forwarding) {
    RandomRatchet_Instance[hemisphere].BaseController(forwarding);
}

void RandomRatchet_View(bool hemisphere) {
    RandomRatchet_Instance[hemisphere].BaseView();
}

void RandomRatchet_OnButtonPress(bool hemisphere) {
    RandomRatchet_Instance[hemisphere].OnButtonPress();
}

void RandomRatchet_OnEncoderMove(bool hemisphere, int direction) {
    RandomRatchet_Instance[hemisphere].OnEncoderMove(direction);
}

void RandomRatchet_ToggleHelpScreen(bool hemisphere) {
    RandomRatchet_Instance[hemisphere].HelpScreen();
}

uint32_t RandomRatchet_OnDataRequest(bool hemisphere) {
    return RandomRatchet_Instance[hemisphere].OnDataRequest();
}

void RandomRatchet_OnDataReceive(bool hemisphere, uint32_t data) {
    RandomRatchet_Instance[hemisphere].OnDataReceive(data);
}


