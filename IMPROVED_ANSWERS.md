# Исправленные ответы на экзаменационные вопросы

## 1. Статический и динамический анализ. Полнота и точность анализа

Статический анализ – анализ исходного кода без выполнения (AST, IR, бинарный код). Динамический анализ – анализ во время выполнения программы.
Полнота (recall) – доля реальных ошибок, которые анализатор обнаруживает. Точность (precision) – доля найденных ошибок, которые действительно являются ошибками.

**Статический анализ:**

Полнота средняя-низкая – исследования показывают, что традиционные статические анализаторы пропускают 47-80% уязвимостей в реальных программах ("An Empirical Study on the Effectiveness of Static C Code Analyzers", анализ 1.15M строк кода, 192 известных уязвимостей). При комбинации нескольких анализаторов пропуски снижаются до 30-69%, но ложные срабатывания увеличиваются на 15 процентных пунктов. 

Точность низкая-средняя – много false positives из-за неполной информации о потоках выполнения. Традиционные инструменты (Clang Static Analyzer, Coverity, PVS-Studio, Cppcheck, Infer) имеют относительно низкую точность. Современные нейросимволические подходы (LLMSA, 2024) могут достигать precision 66.27% и recall 78.57%, но это относится к AI-based анализаторам, а не к традиционным инструментам.

Хорошо находит: неинициализированные переменные, утечки памяти, null pointer dereference, buffer overflow, race conditions, недостижимый код. Плохо находит: ошибки, зависящие от runtime значений, проблемы производительности, логические ошибки.

**Динамический анализ:**

Полнота низкая - только выполняемый код, зависит от покрытия тестами. Эмпирические исследования показывают, что в реальных проектах покрытие обычно составляет 10-40%, но при интенсивном тестировании (model-based automated testing) может достигать 86.4% покрытия строк за 10 минут (исследование JavaScript web applications, 21 benchmark, 18,559 строк кода). Для системного C кода покрытие обычно ниже.

Точность высокая-очень высокая – если ошибка обнаружена, она реально произошла во время выполнения. Хорошо находит: реальные утечки памяти, use-after-free, data races, неопределённое поведение. Плохо находит: недостижимый код, ошибки в непокрытых путях выполнения.

**Инструменты:** статические - Clang Static Analyzer, Coverity, PVS-Studio, Cppcheck, Infer; динамические - Valgrind, AddressSanitizer, ThreadSanitizer, MemorySanitizer, UndefinedBehaviorSanitizer.

**Комбинация подходов:** Использование статического и динамического анализа вместе дает лучшие результаты, так как они компенсируют недостатки друг друга.

---

## 2. Взаимодействие с PCIe устройствами в x86-системах. Posted и non-posted транзакции

PCIe использует последовательную архитектуру с коммутаторами. Взаимодействие происходит через конфигурационное пространство (256/4096 байт), MMIO (Memory-Mapped I/O) и PMIO (Port-Mapped I/O).

**Конфигурационное пространство:** доступ через порты 0xCF8 (CONFIG_ADDRESS) и 0xCFC (CONFIG_DATA) или через ECAM (Enhanced Configuration Access Mechanism) через MMIO. Формат CONFIG_ADDRESS: бит 31 (Enable), биты 23:16 (Bus), биты 15:11 (Device), биты 10:8 (Function), биты 7:2 (Register). Обнаружение устройств: сканирование шин, чтение Vendor ID (offset 0x00), если != 0xFFFF - устройство присутствует.

**BAR (Base Address Registers):** запись 0xFFFFFFFF, чтение возвращает маску размера и типа. Бит 0 = 0 -> MMIO (32/64 бит определяется по биту 2), бит 0 = 1 -> PMIO. Размер = ~(value & mask) + 1. ОС записывает физический адрес в BAR, включает Memory/I/O Space Enable в Command Register.

**Posted транзакции** (Memory Write, Messages): не требуют ответа, инициатор получает ACK на Data Link Layer, но не ждёт завершения на устройстве. Преимущества: низкая задержка, высокая пропускная способность. Недостатки: нет гарантии доставки на уровне транзакций, ошибки доставляются отдельно.

**Non-posted транзакции** (Memory Read, I/O Read/Write, Configuration): требуют Completion от устройства. Инициатор блокируется до получения Completion с данными/подтверждением. Преимущества: гарантия доставки, немедленное обнаружение ошибок. Недостатки: высокая задержка, меньшая пропускная способность.

**Как обычно используется:** Memory Write -> posted (запись в MMIO, bulk transfers, для синхронизации нужны барьеры). Memory Read -> non-posted (чтение MMIO регистров). I/O Read/Write -> non-posted (строгий порядок). Configuration -> non-posted (атомарность). MSI/MSI-X -> posted (быстрая доставка прерываний).

**Барьеры памяти:** posted транзакции могут обгонять non-posted. Для обеспечения порядка используется read-back из MMIO после записи, создающий non-posted транзакцию, которая не может быть обогнана. Это гарантирует, что все предыдущие posted транзакции завершены до продолжения выполнения.

---

## 3. UEFI BootServices и RuntimeServices. Совместимость с ОС

BootServices доступны до ExitBootServices(), RuntimeServices остаются доступными после.

**BootServices:** Memory Services (AllocatePages, GetMemoryMap, AllocatePool), Protocol Services (LocateProtocol, HandleProtocol), Event Services (CreateEvent, SetTimer, WaitForEvent), Image Services (LoadImage, StartImage, ExitBootServices). GetMemoryMap возвращает EFI_MEMORY_DESCRIPTOR с Type, PhysicalStart, VirtualStart, NumberOfPages, Attribute. MapKey необходим для ExitBootServices().

**RuntimeServices:** Time Services (GetTime, SetTime), Variable Services (GetVariable, SetVariable - доступ к NVRAM), Virtual Memory Services (SetVirtualAddressMap, ConvertPointer), Reset Services (Reset).

**Совместимость:** Runtime Services код компилируется для UEFI окружения, ОС работает в своём окружении. Для обеспечения совместимости требуется специальная обработка.

**Решения:**

**SetVirtualAddressMap():** ОС получает memory map через GetMemoryMap(), сохраняет регионы с EFI_MEMORY_RUNTIME (бит 63 в Attribute), создаёт страничную структуру, заполняет VirtualStart для runtime регионов через SetupVirtualAddresses(), вызывает SetVirtualAddressMap(). Прошивка обновляет указатели на виртуальные адреса.

**Calling Convention:** x86-64 первые 4 аргумента в RCX/RDX/R8/R9, остальные на стеке. Для 32-битных сервисов - переключение в 32-битный режим (сохранение сегментов, переключение на GD_KD32, вызов, восстановление на GD_KD).

**Сохранение Runtime памяти:** ОС не переиспользует EFI_RUNTIME_SERVICES_CODE/DATA, отображает в виртуальное пространство с правильными атрибутами (executable для кода, RW для данных). Память должна быть помечена атрибутом EFI_MEMORY_RUNTIME в memory map.

**Virtual Address Change Event:** при SetVirtualAddressMap() генерируется EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE, ОС регистрирует обработчик для обновления указателей через ConvertPointer().

**TPL (Task Priority Level):** Runtime Services НЕ могут вызываться из прерываний (IRQ, TPL_HIGH_LEVEL). Они могут вызываться только из контекста приложения или уведомлений: TPL_APPLICATION (обычный код), TPL_NOTIFY (обработчики событий), TPL_CALLBACK (callbacks). ОС должна использовать правильный TPL при вызове Runtime Services.

**Атрибуты кэширования:** ОС устанавливает EFI_MEMORY_UC/WC/WT/WB согласно атрибутам из memory map для корректной работы MMIO. Эти атрибуты должны быть отражены в page table entries через флаги PTE_PCD (Page Cache Disable) и PTE_PWT (Page Write Through).

**Обновление указателей:** После SetVirtualAddressMap() все указатели на Runtime Services структуры должны быть обновлены через ConvertPointer(). Это включает указатели на EFI_RUNTIME_SERVICES, LoaderParams и другие структуры, содержащие адреса Runtime памяти.

**Практическая реализация в JOS:** bootloader получает memory map, вызывает ExitBootServices(), настраивает VirtualStart через SetupVirtualAddresses(), вызывает SetVirtualAddressMap(). Ядро отображает runtime регионы, сохраняет указатель на EFI_RUNTIME_SERVICES, вызывает сервисы с соблюдением calling convention и правильного TPL.

