import java.util.*

//VARIANT 2
//BRUSILOVA IRINA

class Node(val name: Char, val parent: Node?, val id: Int) {
    val children = mutableMapOf<Char, Node>()
}

class RootedTree{
    var nodeCount = 0
    val root = Node('/', null, nodeCount++)
    val nodesByName = mutableMapOf<Char, MutableList<Node>>()

    init {
        nodesByName.getOrPut('/', { mutableListOf() }).add(root)
    }

    fun addPath(path: String, adj: Array<MutableList<Int>>) {
        var current = root
        for (char in path) {
            if (char == '/') continue
            current = current.children.getOrPut(char) {
                val newNode = Node(char, current, nodeCount++)
                nodesByName.getOrPut(char) { mutableListOf() }.add(newNode)

                adj[current.id].add(newNode.id)
                adj[newNode.id].add(current.id)

                newNode
            }
        }
    }

    fun diameter(adj: Array<MutableList<Int>>, n: Int): Int {
        val root = 0
        fun farthest(start: Int): Pair<Int, IntArray> {
            val d = IntArray(n){-1}
            val q: ArrayDeque<Int> = ArrayDeque()

            d[start] = 0
            q.add(start)

            var far = start

            while(q.isNotEmpty()) {
                val v = q.removeFirst()

                for(u in adj[v]) {
                    if(d[u] == -1) {
                        d[u] = d[v] + 1
                        q.add(u)

                        if(d[u] > d[far])
                            far = u
                    }
                }
            }
            return far to d
        }

        val (u, _) = farthest(root)
        val (v, d) = farthest(u)

        return d[v]
    }
}


fun runTest(paths: List<String>) {
    val tree = RootedTree()
    val n = 100
    val adj = Array(n) { mutableListOf<Int>() }
    for (path in paths) {
        tree.addPath(path, adj)
    }
    println("Диаметр дерева: ${tree.diameter(adj, n)}")

    println()
}


fun main() {
    var paths =listOf<String>();

    println("Тест 1 (Ответ: 2)")
    paths = listOf(
        "/a",
        "/b"
    )
    runTest(paths)

    println("Тест 2 (Ответ: 3)")
    paths = listOf(
        "/a",
        "/b/a",
    )
    runTest(paths)

    println("Тест 3 (Ответ: 4)")
    paths = listOf(
        "/a/b/c",
        "/a/c/x",
        "/d"
    )
    runTest(paths)

    println("Тест 4 (Ответ: 6)")
    paths = listOf(
        "/a/b/c",
        "/a/c",
        "/d/b/x",
        "/d/c"
    )
    runTest(paths)

    println("Тест 5 (Ответ: 6)")
    paths = listOf(
        "/r/a/b/x",
        "/r/a/b/y",
        "/r/a/c/z",
        "/r/a/c/t",
        "/r/d/e/u",
        "/r/d/f/v",
        "/k/q"
    )
    runTest(paths)
}
