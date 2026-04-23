import java.util.*

class Node(val name: Char, val parent: Node?, val id: Int) {
    val children = mutableMapOf<Char, Node>()

    fun getNeighbors(): List<Node> {
        val neighbors = mutableListOf<Node>()
        parent?.let { neighbors.add(it) }
        neighbors.addAll(children.values)
        return neighbors
    }
}

class DirectoryTree {
    private var nodeCount = 0
    val root = Node('/', null, nodeCount++)
    val nodesByName = mutableMapOf<Char, MutableList<Node>>()

    init {
        nodesByName.getOrPut('/', { mutableListOf() }).add(root)
    }

    fun addPath(path: String) {
        var current = root
        for (char in path) {
            if (char == '/') continue
            current = current.children.getOrPut(char) {
                val newNode = Node(char, current, nodeCount++)
                nodesByName.getOrPut(char, { mutableListOf() }).add(newNode)
                newNode
            }
        }
    }

    fun findMinDistance(setA: List<Node>, targetNameB: Char): Int {
        if (setA.isEmpty()) return 0
        if (setA.any { it.name == targetNameB }) return 0

        val queue: Deque<Pair<Node, Int>> = ArrayDeque()
        val visited = BooleanArray(nodeCount)

        for (node in setA) {
            queue.add(node to 0)
            visited[node.id] = true
        }

        while (queue.isNotEmpty()) {
            val (current, dist) = queue.poll()
            if (current.name == targetNameB && setA.none { it == current }) return dist

            for (neighbor in current.getNeighbors()) {
                if (!visited[neighbor.id]) {
                    visited[neighbor.id] = true
                    queue.add(neighbor to dist + 1)
                }
            }
        }
        return 0
    }

    private fun findFurthest(startNode: Node, candidates: List<Node>): Pair<Node, Int> {
        val queue: Deque<Pair<Node, Int>> = ArrayDeque()
        val dists = IntArray(nodeCount) { -1 }
        queue.add(startNode to 0)
        dists[startNode.id] = 0

        var maxD = -1
        var furthestNode = startNode
        val candidateIds = candidates.map { it.id }.toSet()

        while (queue.isNotEmpty()) {
            val (current, dist) = queue.poll()
            if (candidateIds.contains(current.id)) {
                if (dist > maxD) {
                    maxD = dist
                    furthestNode = current
                }
            }
            for (neighbor in current.getNeighbors()) {
                if (dists[neighbor.id] == -1) {
                    dists[neighbor.id] = dist + 1
                    queue.add(neighbor to dist + 1)
                }
            }
        }
        return furthestNode to maxD
    }

    fun findMaxDistance(setA: List<Node>, setB: List<Node>): Int {
        if (setA.isEmpty() || setB.isEmpty()) return 0
        val uTemp = findFurthest(root, setA).first
        val u1 = findFurthest(uTemp, setA).first
        val u2 = findFurthest(u1, setA).first
        return maxOf(findFurthest(u1, setB).second, findFurthest(u2, setB).second)
    }
}


fun runTest(paths: List<String>, a: Char, b: Char) {
    val tree = DirectoryTree()
    for (path in paths) {
        tree.addPath(path)
    }

    val listA = tree.nodesByName[a]
    val listB = tree.nodesByName[b]

    if (listA == null || listB == null) {
        println("One of the sets is empty")
        return
    }

    val minDist = tree.findMinDistance(listA, b)
    val maxDist = tree.findMaxDistance(listA, listB)

    println("Min distance: $minDist")
    println("Max distance: $maxDist")
    println()
}

fun main() {
    var paths =listOf<String>();

    println("Test 1")
    paths = listOf(
        "/a",
        "/b"
    )
    runTest(paths, 'a', 'b')

    println("Test 2")
    paths = listOf(
        "/a",
        "/b/a"
    )
    runTest(paths, 'a', 'b')

    println("Test 3")
    paths = listOf(
        "/a/b/c",
        "/a/c",
        "/d/b/x",
        "/d/c"
    )
    runTest(paths, 'x', 'c')

    println("Test 4")
    paths = listOf(
        "/a",
        "/b/c/c/a",
        "/b/c/a",
        "/b/b/c/b",
        "/b/b/c/c",
        "/b/b/c/a",
        "/b/b",
        "/b/b/c"
    )
    runTest(paths, 'a', 'b')


//    val sc = Scanner(System.`in`)
//    val lines = mutableListOf<String>()
//
//    while (sc.hasNextLine()) {
//        val line = sc.nextLine().trim()
//        if (line.isEmpty()) break
//        lines.add(line)
//    }
//
//    if (lines.size < 2) return
//
//    val lastLine = lines.removeAt(lines.size - 1)
//    val targets = lastLine.split(Regex("\\s+")).filter { it.isNotEmpty() }
//    if (targets.size < 2) return
//    val charA = targets[0][0]
//    val charB = targets[1][0]
//
//    val tree = DirectoryTree()
//    for (path in lines) tree.addPath(path)
//
//    val listA = tree.nodesByName[charA] ?: return
//    val listB = tree.nodesByName[charB] ?: return
//
//    println("${tree.findMinDistance(listA, charB)} ${tree.findMaxDistance(listA, listB)}")
}

