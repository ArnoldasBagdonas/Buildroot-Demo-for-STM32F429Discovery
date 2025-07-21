#ifndef GPIODEXAMPLE_H
#define GPIODEXAMPLE_H

#define GPIO_CHIP "/dev/gpiochip0"

#define GPIO_LINE(port_letter, pin_number)  (((port_letter) - 'A') * 16 + (pin_number))

#define GREEN_LED_LINE   GPIO_LINE('G', 13)
#define RED_LED_LINE     GPIO_LINE('G', 14)
#define BUTTON_LINE      GPIO_LINE('A', 0)

#endif // GPIODEXAMPLE_H