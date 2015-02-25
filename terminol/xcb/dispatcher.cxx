// vi:noai:sw=4
// Copyright © 2014 David Bryant

#include "terminol/xcb/dispatcher.hxx"

void Dispatcher::poll() throw (Error) {
  for (;;) {
    auto event = ::xcb_poll_for_event(_connection);
    if (!event) { break; }

    auto guard        = scopeGuard([event] { std::free(event); });
    auto responseType = XCB_EVENT_RESPONSE_TYPE(event);

    if (responseType != 0) {
      dispatch(responseType, event);
    }
  }

  auto error = ::xcb_connection_has_error(_connection);
  if (error != 0) {
    throw Error("Lost display connection, error=" + stringify(error));
  }
}

void Dispatcher::wait(uint8_t event_type) throw (Error) {
  for (;;) {
    auto event        = ::xcb_wait_for_event(_connection);
    auto guard        = scopeGuard([event] { std::free(event); });
    auto responseType = XCB_EVENT_RESPONSE_TYPE(event);

    if (responseType == 0) {
      ERROR("Zero response type");
      break;      // Because it could be the configure...?
    }
    else {
      dispatch(responseType, event);
      if (responseType == event_type) {
        break;
      }
    }
  }

  auto error = ::xcb_connection_has_error(_connection);
  if (error != 0) {
    throw Error("Lost display connection, error=" + stringify(error));
  }
}

void Dispatcher::dispatch(uint8_t responseType, xcb_generic_event_t * event) {
  switch (responseType) {
    case XCB_KEY_PRESS: {
        auto e = reinterpret_cast<xcb_key_press_event_t *>(event);
        auto i = _observers.find(e->event);
        if (i != _observers.end()) { i->second->keyPress(e); }
        break;
    }
    case XCB_KEY_RELEASE: {
        auto e = reinterpret_cast<xcb_key_release_event_t *>(event);
        auto i = _observers.find(e->event);
        if (i != _observers.end()) { i->second->keyRelease(e); }
        break;
    }
    case XCB_BUTTON_PRESS: {
        auto e = reinterpret_cast<xcb_button_press_event_t *>(event);
        auto i = _observers.find(e->event);
        if (i != _observers.end()) { i->second->buttonPress(e); }
        break;
    }
    case XCB_BUTTON_RELEASE: {
        auto e = reinterpret_cast<xcb_button_release_event_t *>(event);
        auto i = _observers.find(e->event);
        if (i != _observers.end()) { i->second->buttonRelease(e); }
        break;
    }
    case XCB_MOTION_NOTIFY: {
        auto e = reinterpret_cast<xcb_motion_notify_event_t *>(event);
        auto i = _observers.find(e->event);
        if (i != _observers.end()) { i->second->motionNotify(e); }
        break;
    }
    case XCB_EXPOSE: {
        auto e = reinterpret_cast<xcb_expose_event_t *>(event);
        auto i = _observers.find(e->window);
        if (i != _observers.end()) { i->second->expose(e); }
        break;
    }
    case XCB_ENTER_NOTIFY: {
        auto e = reinterpret_cast<xcb_enter_notify_event_t *>(event);
        auto i = _observers.find(e->event);
        if (i != _observers.end()) { i->second->enterNotify(e); }
        break;
    }
    case XCB_LEAVE_NOTIFY: {
        auto e = reinterpret_cast<xcb_leave_notify_event_t *>(event);
        auto i = _observers.find(e->event);
        if (i != _observers.end()) { i->second->leaveNotify(e); }
        break;
    }
    case XCB_FOCUS_IN: {
        auto e = reinterpret_cast<xcb_focus_in_event_t *>(event);
        auto i = _observers.find(e->event);
        if (i != _observers.end()) { i->second->focusIn(e); }
        break;
    }
    case XCB_FOCUS_OUT: {
        auto e = reinterpret_cast<xcb_focus_in_event_t *>(event);
        auto i = _observers.find(e->event);
        if (i != _observers.end()) { i->second->focusOut(e); }
        break;
    }
    case XCB_MAP_NOTIFY: {
        auto e = reinterpret_cast<xcb_map_notify_event_t *>(event);
        auto i = _observers.find(e->event);
        if (i != _observers.end()) { i->second->mapNotify(e); }
        break;
    }
    case XCB_UNMAP_NOTIFY: {
        auto e = reinterpret_cast<xcb_unmap_notify_event_t *>(event);
        auto i = _observers.find(e->event);
        if (i != _observers.end()) { i->second->unmapNotify(e); }
        break;
    }
    case XCB_CONFIGURE_NOTIFY: {
        auto e = reinterpret_cast<xcb_configure_notify_event_t *>(event);
        auto i = _observers.find(e->event);
        if (i != _observers.end()) { i->second->configureNotify(e); }
        break;
    }
    case XCB_DESTROY_NOTIFY: {
        auto e = reinterpret_cast<xcb_destroy_notify_event_t *>(event);
        auto i = _observers.find(e->window);
        if (i != _observers.end()) { i->second->destroyNotify(e); }
        break;
    }
    case XCB_SELECTION_CLEAR: {
        auto e = reinterpret_cast<xcb_selection_clear_event_t *>(event);
        auto i = _observers.find(e->owner);
        if (i != _observers.end()) { i->second->selectionClear(e); }
        break;
    }
    case XCB_SELECTION_NOTIFY: {
        auto e = reinterpret_cast<xcb_selection_notify_event_t *>(event);
        auto i = _observers.find(e->requestor);
        if (i != _observers.end()) { i->second->selectionNotify(e); }
        break;
    }
    case XCB_SELECTION_REQUEST: {
        auto e = reinterpret_cast<xcb_selection_request_event_t *>(event);
        auto i = _observers.find(e->owner);
        if (i != _observers.end()) { i->second->selectionRequest(e); }
        break;
    }
    case XCB_CLIENT_MESSAGE: {
        auto e = reinterpret_cast<xcb_client_message_event_t *>(event);
        auto i = _observers.find(e->window);
        if (i != _observers.end()) { i->second->clientMessage(e); }
        break;
    }
    case XCB_REPARENT_NOTIFY: {
        //PRINT("Got reparent event");
        //auto e = reinterpret_cast<xcb_reparent_notify_event_t *>(event);
        //PRINT("Got reparent, window: " << e->window << ", parent: " << e->parent);
        break;
    }
    case XCB_PROPERTY_NOTIFY: {
        auto e = reinterpret_cast<xcb_property_notify_event_t *>(event);
        auto i = _observers.find(e->window);
        if (i != _observers.end()) { i->second->propertyNotify(e); }
        break;
    }
    default:
        // Ignore any events we aren't interested in.
        break;
  }
}

// I_Selector::I_ReadHandler overrides:

void Dispatcher::handleRead(int fd) throw () {
  ASSERT(fd == xcb_get_file_descriptor(_connection), "");

  try {
    poll();
  }
  catch (const Error & error)
  {
    FATAL(error.message);
  }
}
