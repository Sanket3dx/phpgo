# phpgo - True Go Concurrency for PHP

`phpgo` is a native PHP extension written in Go that brings Go's concurrency primitives to PHP. It enables parallel execution using Goroutines, Channels, WaitGroups, and Select, backed by the Go runtime.

## Features

- **Goroutines**: `phpgo\go(callable)` spawns a lightness Go thread.
- **Channels**: Buffered and unbuffered channels (`phpgo\channel`).
- **Select**: Go-style `select` statement (`phpgo\select`).
- **WaitGroup**: Synchronization primitive (`phpgo\WaitGroup`).

## Requirements

- PHP 8.0+
- Go 1.18+
- GCC / Clang
- **ZTS (Zend Thread Safety)** PHP build is highly recommended for stability.

## Architecture

`phpgo` operates by loading a Go-compiled shared object (`libphpgo.so`) into a thin C-based PHP extension (`phpgo.so`). The Go runtime manages the scheduling of goroutines and channel operations.

## Installation

1. Clone the repository.
2. Build the extension:
   ```bash
   make
   ```
3. Load it in PHP:
   ```bash
   php -d extension=`pwd`/phpgo.so script.php
   ```

## Usage

### Channels & Goroutines
```php
<?php
$ch = phpgo\channel();

phpgo\go(function() use ($ch) {
    echo "Sending from goroutine...\n";
    phpgo\send($ch, "Hello form Go!");
});

$msg = phpgo\receive($ch);
echo "Received: $msg\n";
```

### Select
```php
<?php
$ch1 = phpgo\channel();
$ch2 = phpgo\channel();

// ... spawn producers ...

$result = phpgo\select([
    phpgo\case_recv($ch1),
    phpgo\case_recv($ch2),
    phpgo\case_default(fn() => "Timeout")
]);

var_dump($result['value']);
```

## Safety Warnings

- **NTS Builds**: Running PHP code concurrently (inside `phpgo\go`) on Non-Thread-Safe PHP builds is experimental and may cause crashes if global state is accessed.
- **Resource Management**: Channels are Go objects referenced by ID. They are garbage collected by Go when unreachable, but the ID mapping implies they should be closed explicitly or we rely on the map being cleared (TODO).

## License
MIT
