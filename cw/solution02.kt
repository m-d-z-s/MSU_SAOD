import java.util.*

//class Node(val name: Char, val parent: Node?, val id: Int) {
//    val children = mutableMapOf<Char, Node>()
//}

class DirectoryTree2 {
    private var nodeCount = 0
    val root = Node('/', null, nodeCount++)

    fun addPath(path: String) {
        var current = root
        for (char in path) {
            if (char == '/') continue
            current = current.children.getOrPut(char) {
                Node(char, current, nodeCount++)
            }
        }
    }

    fun countDuplicateSubtrees(): Int {
        val hashCount = mutableMapOf<String, Int>()

        val subtreeHash = mutableMapOf<Int, String>()

        val stack = ArrayDeque<Pair<Node, Boolean>>()
        stack.push(root to false)

        while (stack.isNotEmpty()) {
            val (node, processed) = stack.pop()
            if (processed) {
                val childHashes = node.children.values
                    .map { subtreeHash[it.id]!! }
                    .sorted()

                val hash = buildString {
                    append(node.name)
                    append('(')
                    childHashes.forEachIndexed { i, h ->
                        if (i > 0) append(',')
                        append(h)
                    }
                    append(')')
                }

                subtreeHash[node.id] = hash

                if (node.parent != null) {
                    hashCount[hash] = (hashCount[hash] ?: 0) + 1
                }
            } else {
                stack.push(node to true)
                for (child in node.children.values) {
                    stack.push(child to false)
                }
            }
        }

        return hashCount.values.count { it >= 2 }
    }
}

fun runTest(paths: List<String>) {
    val tree = DirectoryTree2()
    for (path in paths) tree.addPath(path)
    println(tree.countDuplicateSubtrees())
}

fun main() {
    println("Test 1 (expected 0):")
    runTest(listOf("/a", "/b", "/c"))

    println("Test 2 (expected 0):")
    runTest(listOf("/a/a", "/b", "/c"))

    println("Test 3 (expected 1):")
    runTest(listOf("/a/a", "/b", "/c/b"))

    println("Test 4 (expected 3):")
    runTest(listOf("/a/b/x", "/a/c", "/d/b/x", "/d/c"))

    println("Test 5 (expected 3):")
    runTest(listOf("/a/b/x", "/a/b/y", "/c/b/x", "/c/b/y"))

    println("Test 6 (expected 4):")
    runTest(listOf("/a/b/c/x", "/a/b/d", "/e/b/c/x", "/e/b/d"))

    println("Test 7 (expected 7):")
    runTest(listOf(
        "/a/b/c/d/x", "/a/b/c/d/y", "/a/b/e/z", "/a/f",
        "/g/b/c/d/x", "/g/b/c/d/y", "/g/b/e/z", "/g/h",
        "/k/l/d/x", "/k/l/d/y", "/k/l/n/o/p", "/k/l/n/o/q", "/k/r"
    ))

    // Read from stdin (uncomment for actual usage)

//    val sc = Scanner(System.`in`)
//    val lines = mutableListOf<String>()
//    while (sc.hasNextLine()) {
//        val line = sc.nextLine().trim()
//        if (line.isEmpty()) break
//        lines.add(line)
//    }
//    val tree = DirectoryTree2()
//    for (path in lines) tree.addPath(path)
//    println(tree.countDuplicateSubtrees())

}
