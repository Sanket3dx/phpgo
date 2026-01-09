<?php
if (!extension_loaded('phpgo')) exit(1);

echo "Testing WaitGroup...\n";
$wg = new phpgo\WaitGroup();

for ($i = 0; $i < 3; $i++) {
    $wg->add(1);
    phpgo\go(function() use ($wg, $i) {
        echo "Goroutine $i starting\n";
        // We can't easily wait here in PHP context without blocking the Go thread 
        // which might be fine since Go is managing the goroutine.
        echo "Goroutine $i finishing\n";
        $wg->done();
    });
}

echo "Main thread waiting...\n";
$wg->wait();
echo "Main thread resumed! All goroutines finished.\n";
