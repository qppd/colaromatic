#ifndef ALERT_POPUP_H
#define ALERT_POPUP_H

#include <lvgl.h>

enum AlertType { ALERT_INFO, ALERT_WARNING, ALERT_ERROR, ALERT_SUCCESS };

void alertInit();
void showAlert(const char *title, const char *message, AlertType type);
void dismissAlert();
bool alertIsOpen();

/* Called from the UI timer: raises the popups that come from the machine
   itself (link loss, controller refusals, safety trips, endpoint, completion).
   One modal at a time — anything raised while one is open is only kept in the
   diagnostics log. */
void checkAlerts();

#endif  // ALERT_POPUP_H
