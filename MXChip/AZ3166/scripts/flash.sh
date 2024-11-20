#!/bin/bash

openocd -f board/stm32f4discovery.cfg -c "program ../build/app/mxchip_threadx.elf verify reset exit"