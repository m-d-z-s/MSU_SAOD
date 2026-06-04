import java.io.File
import kotlin.math.max

fun buildPrefixFunction(pattern: CharArray): IntArray {
    val m = pattern.size
    val pi = IntArray(m)
    var k = 0
    for (i in 1 until m) {
        while (k > 0 && pattern[k] != pattern[i]) k = pi[k - 1]
        if (pattern[k] == pattern[i]) k++
        pi[i] = k
    }
    return pi
}

fun kmp(str: CharArray, sub: CharArray, pi: IntArray): Int {
    val n = str.size
    val m = sub.size
    if (m == 0) return 0
    var q = 0
    for (i in 0 until n) {
        val c = str[i]
        while (q > 0 && sub[q] != c) q = pi[q - 1]
        if (sub[q] == c) q++
        if (q == m) return i - m + 1
    }
    return -1
}

fun naive(str: CharArray, sub: CharArray): Int {
    val n = str.size
    val m = sub.size
    outer@ for (i in 0..n - m) {
        for (j in 0 until m) {
            if (str[i + j] != sub[j]) continue@outer
        }
        return i
    }
    return -1
}

fun bm(str: CharArray, sub: CharArray): Int {
    val n = str.size
    val m = sub.size
    if (m == 0) return 0
    val badChar = IntArray(256) { -1 }
    for (i in 0 until m) badChar[sub[i].code] = i
    var s = 0
    while (s <= n - m) {
        var j = m - 1
        while (j >= 0 && sub[j] == str[s + j]) j--
        if (j < 0) return s
        val bc = badChar[str[s + j].code]
        s += max(1, j - bc)
    }
    return -1
}

fun benchmarks(
    label: String,
    runs: Int,
    block: () -> Int
): List<Long> {
    val times = mutableListOf<Long>()
    var lastIndex = -1


    repeat(5) { lastIndex = block() }
    repeat(runs) {
        val t0 = System.nanoTime()
        lastIndex = block()
        val t1 = System.nanoTime()
        times += (t1 - t0) / 1_000L
    }
    val avg = times.average()
    val med = times.sorted()[runs / 2]
    println("$label")
    println("  Найдено на позиции : $lastIndex")
    println("  Среднее время      : ${"%.1f".format(avg)} мкс")
    println("  Медиана            : $med мкс")
    println("  Мин / Макс         : ${times.min()} / ${times.max()} мкс")
    println("  Все замеры (мкс)   : ${times.joinToString()}\n")
    return times
}

fun runBenchmarks(str: String, sub: String, runs: Int) {
    println("Подстрока: \"$sub\"\n")
    println("=".repeat(55))

    val strArr = str.toCharArray()
    val subArr = sub.toCharArray()
    val pi     = buildPrefixFunction(subArr)

    val timesNaive = benchmarks("Наивный алгоритм", runs)             { naive(strArr, subArr) }
    val timesKmp   = benchmarks("КМП", runs)                          { kmp(strArr, subArr, pi) }
    val timesBm    = benchmarks("Бойер–Мур", runs)                    { bm(strArr, subArr) }

    println("=".repeat(55))
    println("СВОДНАЯ ТАБЛИЦА (среднее, мкс)")
    println("-".repeat(35))
    println("  %-28s %8.1f".format("Наивный",        timesNaive.average()))
    println("  %-28s %8.1f".format("КМП",            timesKmp.average()))
    println("  %-28s %8.1f".format("Бойер–Мур",      timesBm.average()))
    println("-".repeat(35))

    val csvFile1 = File("average_results.csv")
    csvFile1.bufferedWriter().use { writer ->
        writer.write("naive_average,kmp_average,bm_average\n")
        writer.write(
            "%.1f,%.1f,%.1f\n".format(
                timesNaive.average(),
                timesKmp.average(),
                timesBm.average()
            )
        )
    }

    println("CSV сохранён в: ${csvFile1.absolutePath}")


    val csvFile = File("benchmark_results.csv")
    csvFile.bufferedWriter().use { writer ->
        writer.write("run,naive,kmp,bm\n")
        for (i in timesNaive.indices) {
            writer.write("${i+1},${timesNaive[i]},${timesKmp[i]},${timesBm[i]}\n")
        }
    }
    println("CSV сохранён в: ${csvFile.absolutePath}")
}


fun main() {
    val RUNS = 35
    val sub  = "and is the second single from their greatest hits"

    val filePath = "simplewiki-20260201.txt"
    val file = File(filePath)
    val resourceStream = object {}.javaClass.classLoader.getResourceAsStream(filePath)


    println("Загрузка файла '$filePath'...")
    val str = if (file.exists()) {
        file.readText(Charsets.US_ASCII)
    } else {
        resourceStream!!.bufferedReader(Charsets.US_ASCII).readText()
    }
    println("Загружено ${str.length} символов.\n")

    runBenchmarks(str, sub, RUNS)
}
