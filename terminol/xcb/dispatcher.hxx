// vi:noai:sw=4
// Copyright © 2014 David Bryant

#ifndef XCB__DISPATCHER__HXX
#define XCB__DISPATCHER__HXX

#include "terminol/support/selector.hxx"
#include "terminol/support/pattern.hxx"
#include "terminol/support/conv.hxx"

#include <unordered_map>

#include <xcb/xcb_event.h>
#include <xcb/xcb_aux.h>

class I_Dispatcher {
public:
    class I_Observer {
    public:
        // For the convenience of the subclasses, these virtual methods
        // are not declared abstract.
        virtual void keyPress(xcb_key_press_event_t * UNUSED(event)) noexcept {}
        virtual void keyRelease(xcb_key_release_event_t * UNUSED(event)) noexcept {}
        virtual void buttonPress(xcb_button_press_event_t * UNUSED(event)) noexcept {}
        virtual void buttonRelease(xcb_button_release_event_t * UNUSED(event)) noexcept {}
        virtual void motionNotify(xcb_motion_notify_event_t * UNUSED(event)) noexcept {}
        virtual void mapNotify(xcb_map_notify_event_t * UNUSED(event)) noexcept {}
        virtual void unmapNotify(xcb_unmap_notify_event_t * UNUSED(event)) noexcept {}
        virtual void expose(xcb_expose_event_t * UNUSED(event)) noexcept {}
        virtual void configureNotify(xcb_configure_notify_event_t * UNUSED(event)) noexcept {}
        virtual void focusIn(xcb_focus_in_event_t * UNUSED(event)) noexcept {}
        virtual void focusOut(xcb_focus_out_event_t * UNUSED(event)) noexcept {}
        virtual void enterNotify(xcb_enter_notify_event_t * UNUSED(event)) noexcept {}
        virtual void leaveNotify(xcb_leave_notify_event_t * UNUSED(event)) noexcept {}
        virtual void destroyNotify(xcb_destroy_notify_event_t * UNUSED(event)) noexcept {}
        virtual void selectionClear(xcb_selection_clear_event_t * UNUSED(event)) noexcept {}
        virtual void selectionNotify(xcb_selection_notify_event_t * UNUSED(event)) noexcept {}
        virtual void selectionRequest(xcb_selection_request_event_t * UNUSED(event)) noexcept {}
        virtual void clientMessage(xcb_client_message_event_t * UNUSED(event)) noexcept {}
        virtual void propertyNotify(xcb_property_notify_event_t * UNUSED(event)) noexcept {}
        virtual void reparentNotify(xcb_reparent_notify_event_t * UNUSED(event)) noexcept {}

    protected:
        I_Observer() {}
        virtual ~I_Observer() {}
    };

    virtual void add(xcb_window_t window, I_Observer * observer) = 0;
    virtual void remove(xcb_window_t window) = 0;

protected:
  I_Dispatcher() {}
  ~I_Dispatcher() {}
};

//
//
//

class Dispatcher :
    public    I_Dispatcher,
    protected I_Selector::I_ReadHandler
{
public:
    struct Error {
        explicit Error(const std::string & message_) : message(message_) {}
        std::string message;
    };

    Dispatcher(I_Selector & selector, xcb_connection_t * connection) :
        _selector(selector),
        _connection(connection)
    {
        _selector.addReadable(xcb_get_file_descriptor(_connection), this);
    }

    virtual ~Dispatcher() {
        _selector.removeReadable(xcb_get_file_descriptor(_connection));
    }

    // I_Dispatcher overrides:

    void add (xcb_window_t window, I_Observer * observer) override {
        _observers.insert(std::make_pair(window, observer));
    }

    void remove (xcb_window_t window) override {
        _observers.erase(window);
    }

    // This method is public so that X events can be processed in the
    // absence of the file descriptor becoming readable. Why the descriptor
    // doesn't become readable is a mystery to me.
    void poll () throw (Error);

    void wait(uint8_t event_type) throw (Error);

protected:

    void dispatch(uint8_t responseType, xcb_generic_event_t * event);

    // I_Selector::I_ReadHandler overrides:

    void handleRead(int fd) override;

private:
    typedef std::unordered_map<xcb_window_t, I_Observer *> Observers;

    I_Selector       & _selector;
    xcb_connection_t * _connection;
    Observers          _observers;
};


#endif // XCB__DISPATCHER__HXX
