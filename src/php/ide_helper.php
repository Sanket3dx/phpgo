<?php
/**
 * IDE Helper for phpgo extension
 * 
 * This file is for IDE auto-completion only. 
 * It is not loaded at runtime if the extension is active.
 */

namespace phpgo;

/**
 * Spawn a new Goroutine.
 * 
 * @param callable $func The function to execute concurrently.
 * @return void
 */
function go(callable $func): void {}

/**
 * Create a new Channel.
 * 
 * @param int $buffer Buffer size (0 for unbuffered).
 * @return int Channel ID (Handle).
 */
function channel(int $buffer = 0): int { return 0; }

/**
 * Send a value to a Channel.
 * Blocks if channel is full.
 * 
 * @param int $ch Channel ID.
 * @param mixed $value Value to send.
 * @return bool True on success.
 */
function send(int $ch, mixed $value): bool { return true; }

/**
 * Receive a value from a Channel.
 * Blocks if channel is empty.
 * 
 * @param int $ch Channel ID.
 * @return mixed The received value.
 */
function receive(int $ch): mixed { return null; }

/**
 * Close a Channel.
 * 
 * @param int $ch Channel ID.
 * @return void
 */
function close(int $ch): void {}

/**
 * Select operation on multiple channels.
 * 
 * @param array $cases Array of cases created by case_recv, case_send, case_default.
 * @return array ['index' => int, 'value' => mixed]
 */
function select(array $cases): array { return []; }

/**
 * Create a receive case for select.
 * @param int $ch
 * @return array
 */
function case_recv(int $ch): array { return []; }

/**
 * Create a send case for select.
 * @param int $ch
 * @param mixed $val
 * @return array
 */
function case_send(int $ch, mixed $val): array { return []; }

/**
 * Create a default case for select.
 * @param callable $cb
 * @return array
 */
function case_default(callable $cb): array { return []; }

/**
 * WaitGroup synchronization primitive.
 */
class WaitGroup {
    public function __construct() {}
    public function add(int $delta): void {}
    public function done(): void {}
    public function wait(): void {}
}
