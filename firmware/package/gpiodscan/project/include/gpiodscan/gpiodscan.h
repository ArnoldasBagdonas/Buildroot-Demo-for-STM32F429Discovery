#ifndef GPIODEVENT_H
#define GPIODEVENT_H

#define GPIO_CHIP "/dev/gpiochip0"

#define GPIO_LINE(port_letter, pin_number)  (((port_letter) - 'A') * 16 + (pin_number))

#define GREEN_LED_LINE   GPIO_LINE('G', 13)
#define RED_LED_LINE     GPIO_LINE('G', 14)
#define BUTTON_LINE      GPIO_LINE('A', 0)

#define CONSUMER "gpio-event-example"

#define MAX_EVENTS 20

#endif // GPIODEVENT_H