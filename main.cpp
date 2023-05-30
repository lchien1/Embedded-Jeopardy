/* mbed Microcontroller Library
 * Copyright (c) 2019 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mbed.h"
#include "headers/NHD_0216HZ.h"
#include "headers/Matrix.h"

// The amount of time to wait for people to read the question
#define READING_TIME_SECONDS 5

// The amount of time to wait for a buzz before everyone forfeits
#define AWAITING_TIME_SECONDS 5

// Enable for debug printing
#define DEBUG true

using namespace std::chrono;

BusIn buzzers(ARDUINO_UNO_D9, ARDUINO_UNO_D8, ARDUINO_UNO_D7);
DigitalIn left(D6);
DigitalIn right(D3);
DigitalIn up(D4);
DigitalIn down(D5);
DigitalIn joystick(D2);
NHD_0216HZ lcd(SPI_CS, SPI_MOSI, SPI_SCK);
Matrix matrix(D12);

bool host_pressed_left() {
    return left == 0;
}

bool host_pressed_right() {
    return right == 0;
}

bool host_pressed_up() {
    return up == 0;
}

bool host_pressed_down() {
    return down == 0;
}

bool host_pressed_joystick() {
    return joystick == 0;
}

#define Player1 (0b001)
#define Player2 (0b010)
#define Player3 (0b100)

int poll_buzzers() {
    return buzzers.read() & buzzers.mask();
}

enum State {
    Init,
    Selection,
    NavLeft,
    NavRight,
    NavUp,
    NavDown,
    // Redraw the LED board after moving the "cursor"
    ReturnToSelection,
    HostReading,
    AwaitingBuzz,
    Answer,
};

struct Msg {
    // Potentially not null terminated
    char line1[16];
    char line2[16];

    Msg(const char l1[], const char l2[]) {
        bool finished1 = false;
        bool finished2 = false;
        for (uint8_t i = 0; i < 16; ++i) {
            finished1 = finished1 || (l1[i] == '\0');
            finished2 = finished2 || (l2[i] == '\0');
            line1[i] = finished1 ? '\0' : l1[i];
            line2[i] = finished2 ? '\0' : l2[i];
        }
    }

    Msg(const Msg&) = default;

    void to_global_lcd() const {
        lcd.clr_lcd();
        lcd.set_cursor(0, 0);
        lcd.printf("%.16s", line1);
        lcd.set_cursor(0, 1);
        lcd.printf("%.16s", line2);
    }
};

struct Question {
    bool already_answered;
    Msg question;
    Msg answer;

    Question(Msg question, Msg answer):
        already_answered(false),
        question(question),
        answer(answer) {}

    Question(const Question&) = default;
};

struct Player {
    Timer since_last_click;
    int score;

    Player(): score(0) {
        since_last_click.start();
    }

    // returns true and resets the timer if it's been 1 second since the most
    // recent click, otherwise returns false and doesn't reset the timer.
    auto try_click() -> bool {
        if (duration_cast<seconds>(since_last_click.elapsed_time()).count() >= 1) {
            since_last_click.reset();
            return true;
        }

        return false;
    }
};

Player players[3];

/// A title and three questions
struct Topic {
    const char* title;
    Question questions[3] = {
        Question(Msg("", ""), Msg("", "")),
        Question(Msg("", ""), Msg("", "")),
        Question(Msg("", ""), Msg("", ""))
    };

    Topic(const char title[], Question q1, Question q2, Question q3) {
        this->title = title;
        questions[0] = q1;
        questions[1] = q2;
        questions[2] = q3;
    }

    Topic(const Topic&) = default;
};

struct DisplayState {
    Topic topics[3];

    DisplayState(Topic t1, Topic t2, Topic t3) : topics{t1, t2, t3} {}

    // Draws the colors of each question on the global matrix.
    // Black if the question has been answered, white otherwise.
    void write_on_global_matrix() {
        for (uint8_t row = 0; row < 3; ++row) {
            for (uint8_t col = 0; col < 3; ++col) {
                NeoColor color = is_unanswered(row, col)
                    ? NeoColor(20, 20, 20)
                    : NeoColor(0, 0, 0);

                matrix.fill_rect(
                    color,
                    (col * 2) + 1, // x
                    (row * 2), // y
                    2, // width
                    2 // height
                );
            }
        }
    }

    // Writes the topic and the $$$ on the LCD.
    // row must be in (0, 1, 2, 3) and col in (0, 1, 2)
    void write_nav_prompt_on_lcd(uint8_t row, uint8_t col) {
        lcd.clr_lcd();
        if (row < 3) {
            // Write the topic and prize amount
            lcd.set_cursor(0, 0);
            lcd.printf("%.16s", topics[col].title);
            lcd.set_cursor(0, 1);
            lcd.printf("$%d", 100 * (row + 1));
            if (DEBUG) { printf("%s\n$%d\n", topics[col].title, 100 * (row + 1)); }
        } else if (row == 3) {
            // Write the player score given their col
            lcd.set_cursor(0, 0);
            lcd.printf("Player %d", col + 1);
            lcd.set_cursor(0, 1);
            lcd.printf("$%d", players[col].score);
            if (DEBUG) { printf("Player %d\n$%d\n", col + 1, players[col].score); }

        } else {
            if (DEBUG) { printf("ERROR: ENTERED INVALID STATE\n"); }
        }
    }

    bool is_unanswered(uint8_t row, uint8_t col) {
        return !topics[col].questions[row].already_answered;
    }
};


#define BRIGHTNESS 20

void setup() {
    // Buzzers
    buzzers.mode(PullUp);

    // Host buttons
    left.mode(PullUp);
    right.mode(PullUp);
    up.mode(PullUp);
    down.mode(PullUp);
    joystick.mode(PullUp);

    // LCD
    lcd.init_lcd();
    lcd.clr_lcd();
}

int main() {
    setup();
    // Game state
    State state = Init;

    DisplayState display(
        Topic("Professors",
            Question(////////////////
                Msg("Carr's favorite",
                    "candy?"),
                Msg("Peanut M&Ms", "")
            ),
            Question(////////////////
                Msg("What does",
                    "Molter collect?"),
                Msg("Potato mashers", "")
            ),
            Question(////////////////
                Msg("What online game",
                    "Delano top 1%?"),
                Msg("Hearthstone", "")
            )
        ),
               ////////////////
        Topic("Guess lyrics",
            Question(////////////////
                Msg("I hopped off the ",
                    "plane at LAX"), 
                Msg("Party in the",
                    "U.S.A.")
            ),
            Question(////////////////
                Msg("A tornado flew",
                    "around my room"), 
                Msg("Thinkin Bout You", "")
            ),
            Question(////////////////
                Msg("moms spaghetti", ""), 
                Msg("Lose Yourself", "")
            )
        ),
        Topic("Rhyme Time",
            Question(////////////////
                Msg("An overfed ",
                    "feline"), 
                Msg("Fat Cat", "")
            ),
            Question(////////////////
                Msg("An artificial ",
                    "small horse"), 
                Msg("Phony Pony", "")
            ),
            Question(////////////////
                Msg("Rose colored ",
                    "water basin"), 
                Msg("Pink Sink", "")
            )
        )
    );

    Timer reading_timer;
    Timer awaiting_timer;
    uint8_t who_buzzed_idx;

    // Using (row, col) notation:
    // (0, 0) | (0, 1) | (0, 2)
    // -------+--------+-------
    // (1, 0) | (1, 1) | (1, 2)
    // -------+--------+-------
    // (2, 0) | (2, 1) | (2, 2)
    // -------+--------+-------
    // (3, 0) | (3, 1) | (3, 2)
    uint8_t row = 0;
    uint8_t col = 0;

    uint8_t block_from_buzzing_flags = 0b000;

    while (true) {
        switch (state) {
            case Init: {
                if (DEBUG) { printf("Init\n"); }
                state = ReturnToSelection;
                break;
            } // end Init
            case Selection: {
                if (host_pressed_left()) {
                    state = NavLeft;
                } else if (host_pressed_right()) {
                    state = NavRight;
                } else if (host_pressed_up()) {
                    state = NavUp;
                } else if (host_pressed_down()) {
                    state = NavDown;
                } else if (host_pressed_joystick() && row < 3 && display.is_unanswered(row, col)) {
                    reading_timer.reset();
                    reading_timer.start();
                    if (DEBUG) { printf("Selected question: topic: %s, row: %d\n", display.topics[col].title, row); }

                    // Make the board black
                    matrix.fill(NeoColor());
                    matrix.flush();

                    Msg& q = display.topics[col].questions[row].question;
                    if (DEBUG) { printf("%.16s %.16s\n", q.line1, q.line2); }
                    q.to_global_lcd();

                    state = HostReading;
                }
                break;
            } // end SelectQuestion
            case NavLeft: {
                if (col > 0) {
                    // moving left a question
                    col -= 1;
                }
                state = ReturnToSelection;
                break;
            } // end NavLeft
            case NavRight: {
                if (col < 2) {
                    // moving right a question
                    col += 1;
                }
                state = ReturnToSelection;
                break;
            } // end NavRight
            case NavUp: {
                if (row > 0) {
                    // moving up a question
                    row -= 1;
                }
                state = ReturnToSelection;
                break;
            } // end NavUp
            case NavDown: {
                if (row < 3) {
                    // moving down a question
                    row += 1;
                }
                state = ReturnToSelection;
                break;
            } // end NavDown
            case ReturnToSelection: {
                if (DEBUG) { printf("row: %d, col: %d\n", row, col); }
                
                // Write stuff back to LCD
                display.write_nav_prompt_on_lcd(row, col);

                // Set everything to white
                matrix.fill_rect(NeoColor(20, 20, 20), 0, 0, 8, 8);

                // Fill in the question part of the board
                display.write_on_global_matrix();

                // Draw the "cursor"
                uint8_t cx = (col * 2) + 1;
                uint8_t cy = (row * 2);
                matrix.fill_rect(NeoColor(20, 20, 0), cx, cy, 2, 2);

                // Draw the three player colors
                uint8_t b[3] = { 10, 10, 10 };
                if (row == 3) {
                    b[col] = 30;
                }

                matrix.fill_rect(NeoColor(b[0], 0, 0), 1, 6, 2, 2);
                matrix.fill_rect(NeoColor(0, b[1], 0), 3, 6, 2, 2);
                matrix.fill_rect(NeoColor(0, 0, b[2]), 5, 6, 2, 2);

                // Flush
                matrix.flush();

                state = Selection;
                ThisThread::sleep_for(200ms);
                break;
            } // end ReturnToSelection
            case HostReading: {
                // Check if it's been the proper amount of time
                if (duration_cast<seconds>(reading_timer.elapsed_time()).count() >= READING_TIME_SECONDS) {
                    reading_timer.stop();
                    matrix.fill(NeoColor(20, 20, 20));
                    matrix.flush();
                    Msg& answer = display.topics[col].questions[row].answer;
                    printf("Answer: %.16s %.16s\n", answer.line1, answer.line2);

                    awaiting_timer.reset();
                    awaiting_timer.start();
                    state = AwaitingBuzz;
                }

                // but also prepare to penalize someone...

                uint8_t flags = poll_buzzers();
                if (flags & Player1) {
                    players[0].since_last_click.reset();
                }
                if (flags & Player2) {
                    players[1].since_last_click.reset();
                }
                if (flags & Player3) {
                    players[2].since_last_click.reset();
                }
                break;
            } // end HostReading
            case AwaitingBuzz: {
                if (block_from_buzzing_flags == 0b111
                    || duration_cast<seconds>(awaiting_timer.elapsed_time()).count() >= AWAITING_TIME_SECONDS
                ) {
                    // Everyone failed to answer OR nobody wanted to buzz in
                    display.topics[col].questions[row].already_answered = true;
                    block_from_buzzing_flags = 0b000;
                    state = ReturnToSelection;
                }
                uint8_t flags = poll_buzzers();
                if (flags & Player1 && players[0].try_click() && ((block_from_buzzing_flags & Player1) == 0)) {
                    who_buzzed_idx = 0;
                    matrix.fill(NeoColor(10, 0, 0));
                    matrix.flush();
                    state = Answer;
                }
                if (flags & Player2 && players[1].try_click() && ((block_from_buzzing_flags & Player2) == 0)) {
                    who_buzzed_idx = 1;
                    matrix.fill(NeoColor(0, 10, 0));
                    matrix.flush();
                    state = Answer;
                }
                if (flags & Player3 && players[2].try_click() && ((block_from_buzzing_flags & Player3) == 0)) {
                    who_buzzed_idx = 2;
                    matrix.fill(NeoColor(0, 0, 10));
                    matrix.flush();
                    state = Answer;
                }
                break;
            } // end AwaitingBuzz
            case Answer: {
                // wait until the host either confirms or denies
                int points = 100 * (row + 1);
                if (host_pressed_right()) { // accept
                    players[who_buzzed_idx].score += points;

                    display.topics[col].questions[row].already_answered = true;
                    block_from_buzzing_flags = 0b000;
                    state = ReturnToSelection;
                } else if (host_pressed_left()) { // reject
                    players[who_buzzed_idx].score -= points;
                    block_from_buzzing_flags |= (1 << who_buzzed_idx);
                    matrix.fill(NeoColor(20, 20, 20));
                    matrix.flush();
                    awaiting_timer.reset();
                    awaiting_timer.start();
                    state = AwaitingBuzz;
                }

                // reset player timers if they buzz too early
                // e.g. they know the answer was wrong and try to spam click
                uint8_t flags = poll_buzzers();
                if (flags & Player1) {
                    players[0].since_last_click.reset();
                }
                if (flags & Player2) {
                    players[1].since_last_click.reset();
                }
                if (flags & Player3) {
                    players[2].since_last_click.reset();
                }
                break;
            } // end Answer
        } // end switch
    } // end event loop
} // end main

