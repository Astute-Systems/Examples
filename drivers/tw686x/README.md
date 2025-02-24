# tw686x

Driver for the GXA-1 video capture

# Compile

Run make.

``` .bash
$ make
**************************************************************************
* Building Intersil|Techwell TW6869 driver...                            *
* Type "make help" for a list of available targets.                      *
**************************************************************************
/lib/modules/5.15.148-tegra/build
make -C /lib/modules/`uname -r`/build M="/home/ubuntu/repos/Examples/drivers/tw686x" modules
make[1]: Entering directory '/usr/src/linux-headers-5.15.148-tegra-ubuntu22.04_aarch64/3rdparty/canonical/linux-jammy/kernel-source'
make[1]: Leaving directory '/usr/src/linux-headers-5.15.148-tegra-ubuntu22.04_aarch64/3rdparty/canonical/linux-jammy/kernel-source'
```

# Install

Only need to run ```sudo make unload``` if already loaded.

``` bash
sudo make unload
sudo make install
sudo make load
```

# debugging

Get the module information

``` .bash
$ modinfo  tw6869
filename:       /lib/modules/5.15.148-tegra/kernel/drivers/media/video/tw686x/tw6869.ko
license:        GPL
author:         Ross Newman, Astute Systems
description:    v4l2 driver module for Intersil tw686x. Version: 1.0.1
alias:          pci:v00006000d00000812sv*sd*bc*sc*i*
alias:          pci:v00001797d00006869sv*sd*bc*sc*i*
depends:        videobuf2-v4l2,videodev,videobuf2-vmalloc,videobuf2-common
name:           tw6869
vermagic:       5.15.148-tegra SMP preempt mod_unload modversions aarch64
parm:           vidstd_open:-1 == autodetect 1==NTSC, 2==PAL. Othervalues defualt to autodetect Note: this setting is overriden if driver compiled with ntsc_op =y or pal_op =y (int)
parm:           video_nr:video device number (array of int)
parm:           sfield:single field mode (half size image will not be scaled from the full image (int)
parm:           video_debug:enable debug messages [video] (int)
parm:           gbuffers:number of capture buffers, range 2-32 (int)
parm:           noninterlaced:capture non interlaced video (int)
parm:           irq_debug:enable debug messages [IRQ handler] (int)
parm:           core_debug:enable debug messages [core] (int)
parm:           gpio_tracking:enable debug messages [gpio] (int)
parm:           alsa:enable/disable ALSA DMA sound [dmasound] (int)
parm:           latency:pci latency timer (int)
parm:           vbi_nr:vbi device number (array of int)
parm:           radio_nr:radio device number (array of int)
parm:           tuner:tuner type (array of int)
parm:           card:card type (array of int)
```
