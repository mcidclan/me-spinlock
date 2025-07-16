## Me/Sc Spinlock Sample

A demonstration of a custom mutex implementation using a hardware spinlock for synchronizing shared memory access between the main CPU and the Media Engine.

### Features

- **Independent counters**: Each processor maintains its own counter.
- **Shared memory**: Two shared counters are accessible by both processors.
- **Spinlock synchronization**: Custom hardware mutex implementation to prevent race conditions.

### How it works

The spinlock uses a hardware register (`0xbc100048`) in which:
- Main CPU can only modify bit[0].
- Media Engine can only modify bit[1].
- Exclusive access is guaranteed through custom XOR logic.

Without the spinlock, concurrent access to shared memory would create race conditions.

## build

```bash
make clean; make;
```

## Special Thanks

This sample code wouldn't have been possible without the resources from [uofw on GitHub](https://github.com/uofw/uofw) and the PSP homebrew community, which served as valuable sources of knowledge.
Thanks especially to **crazyc** from ps2dev.org, without whom the use of the Media Engine in the community would be far more difficult.
Thanks to all developers and contributors who have kept the scene alive and to those who continue to do so.

Other indispensable resources:
- [PSPDev on GitHub](https://github.com/pspdev)

*m-c/d*
