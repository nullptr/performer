#include "Config.h"

#include "Ui.h"
#include "Key.h"

#include "core/Debug.h"
#include "core/profiler/Profiler.h"
#include "core/utils/StringBuilder.h"

#include "model/Model.h"

Ui::Ui(Model &model, Engine &engine, Lcd &lcd, ButtonLedMatrix &blm, Encoder &encoder) :
    _model(model),
    _engine(engine),
    _lcd(lcd),
    _blm(blm),
    _encoder(encoder),
    _frameBuffer(CONFIG_LCD_WIDTH, CONFIG_LCD_HEIGHT, _frameBufferData),
    _canvas(_frameBuffer),
    _pageManager(_pages),
    _pageContext({ _messageManager, _pageKeyState, _globalKeyState, _model, _engine }),
    _pages(_pageManager, _pageContext),
    _controllerManager(model, engine)
{
}

void Ui::init() {
    _pageKeyState.reset();
    _globalKeyState.reset();

    _pageManager.setPageSwitchHandler([this] (Page *page) {
        _pageKeyState.reset();
    });

    _pageManager.push(&_pages.top);
    _pages.top.init();
#ifdef CONFIG_ENABLE_INTRO
    _pageManager.push(&_pages.intro);
#endif
    _pageManager.push(&_pages.startup);

    _engine.setMidiReceiveHandler([this] (MidiPort port, uint8_t cable, const MidiMessage &message) {
        if (!_receiveMidiEvents.writable()) {
            DBG("ui midi buffer overflow");
            _receiveMidiEvents.read();
        }
        _receiveMidiEvents.write({ port, cable, message });
        return port == MidiPort::UsbMidi && _controllerManager.isConnected();
    });

    _engine.setUsbMidiConnectHandler([this] (uint16_t vendorId, uint16_t productId) {
        _messageManager.showMessage("USB MIDI DEVICE CONNECTED");
        _controllerManager.connect(vendorId, productId);
    });

    _engine.setUsbMidiDisconnectHandler([this] () {
        _messageManager.showMessage("USB MIDI DEVICE DISCONNECTED");
        _controllerManager.disconnect();
    });

    _engine.setMessageHandler([this] (const char *text, uint32_t duration) {
        _messageManager.showMessage(text, duration);
    });

    _lastFrameBufferUpdateTicks = os::ticks();
    _lastControllerUpdateTicks = os::ticks();
}

void Ui::update() {
    handleKeys();
    handleEncoder();
    handleMidi();

    // abort if track engines are not consistent with model
    if (!_engine.trackEnginesConsistent()) {
        return;
    }

    _leds.clear();
    _pageManager.updateLeds(_leds);
    _blm.setLeds(_leds.array());

    // update display at target fps
    uint32_t currentTicks = os::ticks();
    uint32_t intervalTicks = os::time::ms(1000 / _pageManager.fps());
    if (currentTicks - _lastFrameBufferUpdateTicks >= intervalTicks) {
        _pageManager.draw(_canvas);
        _messageManager.update();
        _messageManager.draw(_canvas);
        _lcd.draw(_frameBuffer.data());
        _lastFrameBufferUpdateTicks += intervalTicks;
    }

    intervalTicks = os::time::ms(1000 / _controllerManager.fps());
    if (currentTicks - _lastControllerUpdateTicks >= intervalTicks) {
        if (!_engine.isSuspended()) {
            _controllerManager.update();
        }
        _lastControllerUpdateTicks += intervalTicks;
    }
}

void Ui::showAssert(const char *filename, int line, const char *msg) {
    _canvas.setColor(0);
    _canvas.fill();

    _canvas.setColor(0xf);
    _canvas.setFont(Font::Small);
    _canvas.drawText(4, 10, "FATAL ERROR");

    _canvas.setFont(Font::Tiny);
    _canvas.drawTextMultiline(4, 20, CONFIG_LCD_WIDTH - 8, msg);

    if (filename) {
        FixedStringBuilder<128> str("%s:%d", filename, line);
        _canvas.drawTextMultiline(4, 40, CONFIG_LCD_WIDTH - 8, str);
    }

    _canvas.drawText(4, 58, "PRESS ENCODER TO RESET");

    _lcd.draw(_frameBuffer.data());
}

static uint8_t lastKeyPressed[] = {0, 0};

// Using the scancodes defined at https://www.win.tue.nl/~aeb/linux/kbd/scancodes-14.html.
// This mapping is also likely something others would want to change, as it is very specific to
// taste and underlying layout (Colemak-DH in my case).
static int8_t keyMap[] = {
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::Step8,
    Key::Code::Play,
    Key::Code::Step3,
    Key::Code::Step4,
    Key::Code::Step13,
    Key::Code::Track2,
    Key::Code::Pattern,
    Key::Code::Step5,
    Key::Code::Step14,
    Key::Code::Tempo,
    Key::Code::Performer,
    Key::Code::Track4,
    Key::Code::Right,
    Key::Code::Step12,
    Key::Code::Step15,
    Key::Code::Track3,
    Key::Code::Track0,
    Key::Code::Step9,
    Key::Code::Step10,
    Key::Code::Step11,
    Key::Code::Track5,
    Key::Code::Left,
    Key::Code::Track1,
    Key::Code::Step2,
    Key::Code::Track6,
    Key::Code::Step1,
    Key::Code::F0,
    Key::Code::F1,
    Key::Code::F2,
    Key::Code::F3,
    Key::Code::F4,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::Track7,
    Key::Code::None,
    Key::Code::None,
    Key::Code::Step6,
    Key::Code::Step7,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::None,
    Key::Code::Shift,
    Key::Code::Page,
    Key::Code::None,
};

void Ui::handleKeys() {
    const auto processEvent = [this](ButtonLedMatrix::Event& event) {
        bool isDown = event.action() == ButtonLedMatrix::Event::KeyDown;
        _pageKeyState[event.value()] = isDown;
        _globalKeyState[event.value()] = isDown;
        Key key(event.value(), _globalKeyState);
        KeyEvent keyEvent(isDown ? Event::KeyDown : Event::KeyUp, key);
        _pageManager.dispatchEvent(keyEvent);
        if (isDown) {
            KeyPressEvent keyPressEvent = _keyPressEventTracker.process(key);
            _pageManager.dispatchEvent(keyPressEvent);
        }
    };

    ButtonLedMatrix::Event event{};
    while (_blm.nextEvent(event)) {
        processEvent(event);
    }
    while (!keyboard.empty()) {
        const auto k = keyboard.read();
        const auto k1 = keyMap[k & 0xff];
        const auto k2 = keyMap[(k >> 16) & 0xff];
        if (k1 != Key::Code::None) {
            // Check if the first last key was released
            if (lastKeyPressed[0] && lastKeyPressed[0] != k1) {
                event = ButtonLedMatrix::Event(ButtonLedMatrix::Event::KeyUp, lastKeyPressed[0]);
                processEvent(event);
            }
            event = ButtonLedMatrix::Event(ButtonLedMatrix::Event::KeyDown, k1);
            processEvent(event);
            lastKeyPressed[0] = k1;

            if (k2 != Key::Code::None) {
                // Check if the second last key was released
                if (lastKeyPressed[1] && lastKeyPressed[1] != k1 && lastKeyPressed[1] != k2) {
                    event = ButtonLedMatrix::Event(ButtonLedMatrix::Event::KeyUp, lastKeyPressed[1]);
                    processEvent(event);
                }
                event = ButtonLedMatrix::Event(ButtonLedMatrix::Event::KeyDown, k2);
                processEvent(event);
                lastKeyPressed[1] = k2;
            } else {
                if (lastKeyPressed[1]) {
                    event = ButtonLedMatrix::Event(ButtonLedMatrix::Event::KeyUp, lastKeyPressed[1]);
                    processEvent(event);
                    lastKeyPressed[1] = 0;
                }
            }
        } else {
            // No key presses, clear everything
            if (lastKeyPressed[1]) {
                event = ButtonLedMatrix::Event(ButtonLedMatrix::Event::KeyUp, lastKeyPressed[1]);
                processEvent(event);
                lastKeyPressed[1] = 0;
            }
            if (lastKeyPressed[0]) {
                event = ButtonLedMatrix::Event(ButtonLedMatrix::Event::KeyUp, lastKeyPressed[0]);
                processEvent(event);
                lastKeyPressed[0] = 0;
            }
        }
    }
}

void Ui::handleEncoder() {
    Encoder::Event event;
    while (_encoder.nextEvent(event)) {
        switch (event) {
        case Encoder::Left:
        case Encoder::Right: {
            EncoderEvent encoderEvent(event == Encoder::Left ? -1 : 1, _pageKeyState[Key::Encoder]);
            _pageManager.dispatchEvent(encoderEvent);
            break;
        }
        case Encoder::Down:
        case Encoder::Up: {
            bool isDown = event == Encoder::Down;
            _pageKeyState[Key::Encoder] = isDown ? 1 : 0;
            _globalKeyState[Key::Encoder] = isDown ? 1 : 0;
            Key key(Key::Encoder, _globalKeyState);
            KeyEvent keyEvent(isDown ? Event::KeyDown : Event::KeyUp, key);
            _pageManager.dispatchEvent(keyEvent);
            if (isDown) {
                KeyPressEvent keyPressEvent = _keyPressEventTracker.process(key);
                _pageManager.dispatchEvent(keyPressEvent);
            }
            break;
        }
        }
    }
}

void Ui::handleMidi() {
    while (_receiveMidiEvents.readable()) {
        auto receiveEvent = _receiveMidiEvents.read();
        if (!_controllerManager.recvMidi(receiveEvent.port, receiveEvent.cable, receiveEvent.message)) {
            // only process events from cable 0
            if (receiveEvent.cable == 0) {
                MidiEvent midiEvent(receiveEvent.port, receiveEvent.message);
                _pageManager.dispatchEvent(midiEvent);
            }
        }
    }
}
