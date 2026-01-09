<?php
if (!extension_loaded('phpgo')) {
    dl('phpgo.so');
}

echo "Correctly loaded phpgo\n";

// Test Channel
$ch = phpgo\channel(2);
echo "Created channel $ch\n";

phpgo\send($ch, "Hello");
phpgo\send($ch, 123);
echo "Sent values\n";

$val1 = phpgo\receive($ch);
echo "Received: $val1\n";

$val2 = phpgo\receive($ch);
echo "Received: $val2\n";

phpgo\close($ch);
echo "Closed channel\n";

// Test Select
$ch1 = phpgo\channel(1);
$ch2 = phpgo\channel(1);

phpgo\send($ch1, "Message 1");
// ch2 is empty

$result = phpgo\select([
    phpgo\case_recv($ch1),
    phpgo\case_recv($ch2),
    phpgo\case_default(function() { return "Default"; })
]);

echo "Select Result Index: " . $result['index'] . "\n";
echo "Select Result Value: " . $result['value'] . "\n";

?>
