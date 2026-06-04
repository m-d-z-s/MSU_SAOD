import kotlinx.coroutines.*
import kotlinx.coroutines.channels.*
import java.io.FileWriter
import java.util.Random
import java.util.concurrent.atomic.AtomicLong
import kotlin.system.measureTimeMillis

typealias BigInt = Long

// ─────────────────────────────────────────────
// Вычислительные функции
// ─────────────────────────────────────────────

fun fibonacci(n: BigInt): BigInt = if (n < 2) n else fibonacci(n - 1) + fibonacci(n - 2)

fun special(n: Int): Boolean {
    var sum: BigInt = 0
    for (i in 0 until n) sum += fibonacci(i.toLong())
    return sum % 2 == 0L
}

// ─────────────────────────────────────────────
// Генерация данных
// ─────────────────────────────────────────────

fun generateData(size: Int, seed: Long = 1L): List<Int> {
    val rng = Random(seed)
    fun poissonSample(lambda: Double): Int {
        val l = Math.exp(-lambda)
        var k = 0; var p = 1.0
        do { k++; p *= rng.nextDouble() } while (p > l)
        return k - 1
    }
    return List(size) {
        val value = 40 + poissonSample(4.0)
        if (value < 53) value else 53
    }
}

// ─────────────────────────────────────────────
// Подход 0: Однопоточный (baseline)
// ─────────────────────────────────────────────

fun single(v: List<Int>): Long = v.count { special(it) }.toLong()

// ─────────────────────────────────────────────
// Подход 1: Block
// ─────────────────────────────────────────────

fun block(v: List<Int>, nThreads: Int): Long = runBlocking {
    val partSize = v.size / nThreads
    val jobs = (0 until nThreads).map { t ->
        val a = t * partSize
        val b = if (t == nThreads - 1) v.size else a + partSize
        async(Dispatchers.Default) {
            var cnt = 0L
            for (i in a until b) if (special(v[i])) cnt++
            cnt
        }
    }
    jobs.sumOf { it.await() }
}

// ─────────────────────────────────────────────
// Подход 2: Channel Queue — динамическая очередь задач
// ─────────────────────────────────────────────

fun channelQueue(v: List<Int>, nThreads: Int): Long = runBlocking {
    val taskChannel = Channel<Int>(capacity = Channel.UNLIMITED)
    val resultChannel = Channel<Long>(capacity = Channel.UNLIMITED)

    launch(Dispatchers.Default) {
        for (item in v) taskChannel.send(item)
        taskChannel.close()
    }

    val workers = (0 until nThreads).map {
        launch(Dispatchers.Default) {
            for (item in taskChannel) {
                resultChannel.send(if (special(item)) 1L else 0L)
            }
        }
    }

    launch(Dispatchers.Default) {
        workers.forEach { it.join() }
        resultChannel.close()
    }

    var total = 0L
    for (r in resultChannel) total += r
    total
}

// ─────────────────────────────────────────────
// Подход 3: Channel + AtomicCounter
//   Динамическая очередь через Channel
// ─────────────────────────────────────────────

fun channelAtomic(v: List<Int>, nThreads: Int): Long = runBlocking {
    val taskChannel = Channel<Int>(capacity = Channel.UNLIMITED)
    val counter = AtomicLong(0)

    launch(Dispatchers.Default) {
        for (item in v) taskChannel.send(item)
        taskChannel.close()
    }

    val workers = (0 until nThreads).map {
        launch(Dispatchers.Default) {
            for (item in taskChannel) {
                if (special(item)) counter.incrementAndGet()
            }
        }
    }
    workers.forEach { it.join() }
    counter.get()
}

// ─────────────────────────────────────────────
// Подход 4: Channel с батчингом (Chunked Queue)
//   Динамическая очередь, но задачи группируются
//   в батчи размером chunkSize.
// ─────────────────────────────────────────────

fun channelChunked(v: List<Int>, nThreads: Int, chunkSize: Int = 4): Long = runBlocking {
    val taskChannel = Channel<List<Int>>(capacity = Channel.UNLIMITED)
    val counter = AtomicLong(0)

    launch(Dispatchers.Default) {
        v.chunked(chunkSize).forEach { chunk -> taskChannel.send(chunk) }
        taskChannel.close()
    }

    val workers = (0 until nThreads).map {
        launch(Dispatchers.Default) {
            for (chunk in taskChannel) {
                var localCnt = 0L
                for (item in chunk) if (special(item)) localCnt++
                if (localCnt > 0) counter.addAndGet(localCnt)
            }
        }
    }
    workers.forEach { it.join() }
    counter.get()
}

// ─────────────────────────────────────────────
// Замер времени
// ─────────────────────────────────────────────

data class BenchResult(
    val nThreads: Int,
    val approach: String,
    val timeMs: Long,
    val result: Long
)

fun measureApproach(
    name: String,
    nThreads: Int,
    runs: Int = 3,
    fn: () -> Long
): BenchResult {
    var best = Long.MAX_VALUE
    var res = 0L
    repeat(runs) {
        val t = measureTimeMillis { res = fn() }
        if (t < best) best = t
    }
    return BenchResult(nThreads, name, best, res)
}

// ─────────────────────────────────────────────
// CSV-save
// ─────────────────────────────────────────────

fun saveResultsCsv(path: String, results: List<BenchResult>) {
    FileWriter(path).use { fw ->
        fw.write("n_threads,approach,time_ms,result\n")
        for (r in results) fw.write("${r.nThreads},${r.approach},${r.timeMs},${r.result}\n")
    }
    println("Saved: $path")
}

fun saveSpeedupCsv(path: String, singleTime: Long, results: List<BenchResult>) {
    FileWriter(path).use { fw ->
        fw.write("n_threads,approach,speedup,efficiency\n")
        for (r in results) {
            val speedup = singleTime.toDouble() / r.timeMs
            val efficiency = speedup / r.nThreads
            fw.write("${r.nThreads},${r.approach},${"%.4f".format(speedup)},${"%.4f".format(efficiency)}\n")
        }
    }
    println("Saved: $path")
}

// ─────────────────────────────────────────────
// main
// ─────────────────────────────────────────────

fun main() {
    val dataSize = 50
    val threadCounts = listOf(1, 2, 3, 4, 5)
    val v = generateData(dataSize)

    println("Data ($dataSize elements): ${v.take(20)}...")
    println()

    val allResults = mutableListOf<BenchResult>()

    // ── Однопоточный baseline ──────────────────
    println("Running single-thread baseline...")
    val singleResult = measureApproach("single", 1) { single(v) }
    println("  single(1): ${singleResult.timeMs} ms  →  count = ${singleResult.result}")
    val singleTime = singleResult.timeMs
    allResults.add(singleResult)

    // ── Многопоточные подходы ─────────────────
    //   block          — статическое распределение (для сравнения)
    //   channel_queue  — динамическая очередь + resultChannel
    //   channel_atomic — динамическая очередь + AtomicLong
    //   channel_chunked— динамическая очередь с батчингом
    val approaches = listOf(
        "block"           to { n: Int -> block(v, n) },
        "channel_queue"   to { n: Int -> channelQueue(v, n) },
        "channel_atomic"  to { n: Int -> channelAtomic(v, n) },
        "channel_chunked" to { n: Int -> channelChunked(v, n) }
    )

    for (nThreads in threadCounts.drop(1)) {   // 2..5
        println("\n── $nThreads threads ──────────────────────────────")
        for ((name, fn) in approaches) {
            val r = measureApproach(name, nThreads) { fn(nThreads) }
            val speedup = singleTime.toDouble() / r.timeMs
            val eff     = speedup / nThreads
            println("  %-20s %6d ms  speedup=%5.2fx  eff=%5.3f  count=%d"
                .format(r.approach, r.timeMs, speedup, eff, r.result))
            allResults.add(r)
        }
    }

    // ── Сохранение CSV ───────────────────────
    saveResultsCsv("results.csv", allResults)
    saveSpeedupCsv("speedup.csv", singleTime, allResults.filter { it.nThreads > 1 })

    println("\nDone. Check results.csv and speedup.csv")
}
