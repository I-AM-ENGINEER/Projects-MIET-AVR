# Сборник решенных практико-ориентированных проектов Шепелева

Все работы сделаны в PlatformIO, для симуляции используется Proteus 8.16 SP3 (Build 36097).
Если готовы предоставить свои работы в сборник - кидайте pullrequest, или пишите на почту

Все проекты сделаны по одной структуре:

* docs - документация на контроллер и используемую периферию
* simulation - проект для симуляции Proteus 8
* src - исходный код

### Сделанные проекты:

2\. Собрать схему часов и разработать программу, используя семи сегментный индикатор типа 7SEG-BCD и дополнительные светодиоды при необходимости, для отсчета времени использовать внешнюю микросхему RTC DS3231 или аналогичную. Реализовать функцию будильника, реализовать функцию таймера Переключение осуществлять при помощи кнопок

  https://github.com/I-AM-ENGINEER/Clock7SEG-AVR

5\. Собрать схему термометра с двумя датчиками, используя в качестве датчика температуры терморезистор, вывод данных реализовать на семи сегментный индикатор типа 7SEG-BCD. Для отображения температуры задействовать 4 разряда. На схеме предусмотреть кнопку переключающую режим отображения температуры (С\F), кнопку переключения между датчиками, вывод минимальной и максимальной температуры.

  https://github.com/I-AM-ENGINEER/ThermistorsReader-AVR

5\. Собрать схему термометра с двумя датчиками, используя в качестве датчика температуры терморезистор, вывод данных реализовать на семи сегментный индикатор типа 7SEG-BCD. Для отображения температуры задействовать 4 разряда. На схеме предусмотреть кнопку переключающую режим отображения температуры (С\F), кнопку переключения между датчиками, вывод минимальной и максимальной температуры.

  https://github.com/I-AM-ENGINEER/ThermistorsReader-AVR

9\. Собрать схему часов и составить программу вывода на жидкокристаллический дисплей типа PCD8544, используя встроенный в микроконтроллер таймер. Реализовать функцию будильника, реализовать установку времени через терминал(UART).
  
  https://github.com/I-AM-ENGINEER/ClockPCD8544-AVR

16\. Собрать схему кодового замка на матричной клавиатуре (4x4) и составить программу закрытия или открытия электромеханического реле в зависимости от введенного кода. Для открытия замка должна быть предусмотрена комбинация последовательно нажатых кнопок. Реализовать режим смены комбинации после открытия замка, реализовать вывод цифр на дисплей.

  https://github.com/I-AM-ENGINEER/CodeLock-AVR

17\. Собрать схему и составить программу управления скоростью мотора постоянного тока с помощью матричной клавиатуры (3х3). Скорость и направление вращения задается нажатием кнопок или их сочетанием. Мотор приводится в движение при помощи микросхемы L298 или аналогичной.

  https://github.com/I-AM-ENGINEER/MotorController-AVR

20\. Собрать схему и составить программу интерпретатор, преобразующего аналоговый сигнал в ШИМ сигнал определенной скважности для управления положением стандартного сервопривода. В программе предусмотреть фильтрацию сигнала и ограничение на угол поворота сервопривода, параметры аналогового и ШИМ сигнала вывести на дисплее.

 https://github.com/I-AM-ENGINEER/ServoADC-AVR

21\. Собрать схему и составить программу интерпретатор преобразующий команду, поступившую по интерфейсу обмена UART, в ШИМ сигнал определенной скважности для управления положением стандартного сервопривода. В программе предусмотреть фильтрацию сигнала и ограничение на угол поворота сервопривода, принятую команду и параметры ШИМ сигнала вывести на дисплее.

  https://github.com/I-AM-ENGINEER/ServoUART-AVR

22\. Реализовать схему измерения тока до 3.5А на электродвигателе с выводом данных на дисплей. Можно использовать специализированные микросхемы.

  https://github.com/I-AM-ENGINEER/CurrentSensor-AVR

### Если вашей темы нет в списке - принимаю заказы на выполнение (sdwad6@gmail.com).
