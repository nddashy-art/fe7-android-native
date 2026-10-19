package org.fe7android.nativeport

import android.app.Activity
import android.content.Intent
import android.graphics.Bitmap
import android.net.Uri
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.MotionEvent
import android.provider.OpenableColumns
import android.view.Gravity
import android.view.View
import android.widget.FrameLayout
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import java.io.IOException
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.Locale

/**
 * Prototype launcher with a user-selected ROM document.
 * The ROM remains at its chosen location; this app does not upload or copy it.
 * Native Android launcher and mGBA-backed GBA gameplay screen.
 */
class MainActivity : Activity() {
    private lateinit var romStatus: TextView
    private val frameHandler = Handler(Looper.getMainLooper())
    private var gameBitmap: Bitmap? = null
    private var gameImage: ImageView? = null
    private var gameLoopActive = false
    private val gameFrameBuffer: ByteBuffer = ByteBuffer.allocateDirect(GAME_FRAME_WIDTH * GAME_FRAME_HEIGHT * 4)
        .order(ByteOrder.nativeOrder())

    companion object {
        private const val REQUEST_OPEN_ROM = 4107
        private const val PREFS = "fe7_settings"
        private const val KEY_ROM_URI = "selected_rom_uri"
        private const val KEY_ROM_NAME = "selected_rom_name"
        private const val GBA_HEADER_SIZE = 0xB0
        private const val MIN_GBA_SIZE = GBA_HEADER_SIZE
        private const val TEST_FRAME_WIDTH = 240
        private const val TEST_FRAME_HEIGHT = 160
        private const val GAME_FRAME_WIDTH = 240
        private const val GAME_FRAME_HEIGHT = 160
    }

    init {
        System.loadLibrary("fe7native")
    }

    private external fun nativeGetBridgeStatus(): String
    private external fun nativeInspectGbaHeader(header: ByteArray): String
    private external fun nativeCreateTestFrame(): IntArray
    private external fun nativeStartGame(romPath: String): String
    private external fun nativeRunFrame(): IntArray
    private external fun nativeRunFrameInto(outputBuffer: ByteBuffer): Boolean
    private external fun nativeSetButtons(buttons: Int)
    private external fun nativeStopGame()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        showLauncher()
    }

    private fun showLauncher() {
        gameLoopActive = false
        frameHandler.removeCallbacksAndMessages(null)
        window.decorView.systemUiVisibility = 0

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
            setPadding(28, 24, 28, 24)
            setBackgroundColor(0xFF151923.toInt())
        }

        val title = TextView(this).apply {
            text = "FE7 ANDROID NATIVE"
            textSize = 26f
            gravity = Gravity.CENTER
            setTextColor(0xFFFFD98A.toInt())
        }

        val description = TextView(this).apply {
            text = "Selecciona tu propia ROM de Fire Emblem: The Blazing Blade (US).\n\nLa app no incluye una ROM ni la sube a internet."
            textSize = 16f
            gravity = Gravity.CENTER
            setTextColor(0xFFF1F3F8.toInt())
            setLineSpacing(6f, 1.0f)
        }

        val chooseButton = Button(this).apply {
            text = "Seleccionar ROM (.gba)"
            setOnClickListener { openRomPicker() }
        }

        val startGameButton = Button(this).apply {
            text = "Iniciar FE7 (motor mGBA)"
            setOnClickListener { startSelectedGame() }
        }

        val videoTestButton = Button(this).apply {
            text = "Probar pantalla nativa"
            setOnClickListener { showNativeVideoTest() }
        }

        romStatus = TextView(this).apply {
            textSize = 14f
            gravity = Gravity.CENTER
            setTextColor(0xFFB7C0D4.toInt())
            setLineSpacing(5f, 1.0f)
        }

        val warning = TextView(this).apply {
            text = "${nativeGetBridgeStatus()}\n\nMotor mGBA integrado en esta versión experimental. Si no arranca, revisa el mensaje al iniciar."
            textSize = 12f
            gravity = Gravity.CENTER
            setTextColor(0xFFFFD98A.toInt())
            setLineSpacing(4f, 1.0f)
        }

        root.addView(title, layoutParams(bottom = 22))
        root.addView(description, layoutParams(bottom = 18))
        root.addView(chooseButton, layoutParams(bottom = 8))
        root.addView(startGameButton, layoutParams(bottom = 8))
        root.addView(videoTestButton, layoutParams(bottom = 8))
        root.addView(romStatus, layoutParams(bottom = 18))
        root.addView(warning, layoutParams())

        setContentView(root)
        restoreSavedRom()
    }


    private fun startSelectedGame() {
        val uriText = getSharedPreferences(PREFS, MODE_PRIVATE).getString(KEY_ROM_URI, null)
        if (uriText == null) {
            romStatus.text = "Primero selecciona tu archivo .gba."
            return
        }

        try {
            val localRom = java.io.File(cacheDir, "selected-fe7.gba")
            contentResolver.openInputStream(Uri.parse(uriText))?.use { input ->
                localRom.outputStream().use { output -> input.copyTo(output) }
            } ?: run {
                romStatus.text = "No se pudo leer el archivo seleccionado."
                return
            }

            val error = nativeStartGame(localRom.absolutePath)
            if (error.isNotEmpty()) {
                romStatus.text = "No se pudo iniciar el juego: $error"
                return
            }
            showGameScreen()
        } catch (error: Exception) {
            romStatus.text = "No se pudo preparar la ROM: ${error.javaClass.simpleName}"
        }
    }

    private fun showGameScreen() {
        frameHandler.removeCallbacksAndMessages(null)
        currentPressedButtons = 0
        nativeSetButtons(0)
        gameLoopActive = true

        // Fullscreen landscape; touch controls float over the image edges.
        window.decorView.systemUiVisibility = (
            View.SYSTEM_UI_FLAG_FULLSCREEN
                or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                or View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
        )

        val root = FrameLayout(this).apply { setBackgroundColor(0xFF05070B.toInt()) }
        val image = ImageView(this).apply {
            scaleType = ImageView.ScaleType.FIT_CENTER
            setBackgroundColor(0xFF05070B.toInt())
            contentDescription = "Pantalla de Fire Emblem ejecutada por mGBA"
        }
        val bitmap = Bitmap.createBitmap(GAME_FRAME_WIDTH, GAME_FRAME_HEIGHT, Bitmap.Config.ARGB_8888)
        gameBitmap = bitmap
        gameImage = image
        image.setImageBitmap(bitmap)
        root.addView(image, FrameLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT
        ))

        val controls = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(8, 4, 8, 4)
            setBackgroundColor(0x66080B12)
        }
        val directions = LinearLayout(this).apply { gravity = Gravity.CENTER }
        addGameButton(directions, "↑", 4)
        addGameButton(directions, "↓", 5)
        addGameButton(directions, "←", 6)
        addGameButton(directions, "→", 7)
        val actions = LinearLayout(this).apply { gravity = Gravity.CENTER }
        addGameButton(actions, "B", 0)
        addGameButton(actions, "A", 8)
        addGameButton(actions, "SEL", 2)
        addGameButton(actions, "STA", 3)
        controls.addView(directions, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        controls.addView(actions, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        root.addView(controls, FrameLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.BOTTOM
        ))

        val back = Button(this).apply {
            text = "×"
            textSize = 20f
            contentDescription = "Detener juego y volver al selector"
            setPadding(0, 0, 0, 0)
            alpha = 0.78f
            setOnClickListener {
                gameLoopActive = false
                frameHandler.removeCallbacksAndMessages(null)
                nativeSetButtons(0)
                nativeStopGame()
                gameBitmap = null
                gameImage = null
                showLauncher()
            }
        }
        root.addView(back, FrameLayout.LayoutParams(dp(48), dp(44), Gravity.TOP or Gravity.END))
        setContentView(root)
        renderGameFrame()
    }

    private fun dp(value: Int): Int =
        (value * resources.displayMetrics.density + 0.5f).toInt()

    private fun addGameButton(parent: LinearLayout, label: String, retroButtonId: Int) {
        val button = Button(this).apply {
            text = label
            textSize = if (label.length > 2) 10f else 14f
            minHeight = dp(42)
            minWidth = dp(42)
            setPadding(4, 2, 4, 2)
            alpha = 0.82f
            setOnTouchListener { _, event ->
                val mask = 1 shl retroButtonId
                when (event.actionMasked) {
                    MotionEvent.ACTION_DOWN -> {
                        currentPressedButtons = currentPressedButtons or mask
                        nativeSetButtons(currentPressedButtons)
                    }
                    MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                        currentPressedButtons = currentPressedButtons and mask.inv()
                        nativeSetButtons(currentPressedButtons)
                    }
                }
                true
            }
        }
        parent.addView(button, LinearLayout.LayoutParams(
            0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f
        ).apply {
            marginStart = 2
            marginEnd = 2
        })
    }

    private var currentPressedButtons: Int = 0

    private fun renderGameFrame() {
        if (!gameLoopActive) return
        try {
            val bitmap = gameBitmap
            val image = gameImage
            if (bitmap != null && image != null) {
                gameFrameBuffer.clear()
                if (nativeRunFrameInto(gameFrameBuffer)) {
                    gameFrameBuffer.position(0)
                    bitmap.copyPixelsFromBuffer(gameFrameBuffer)
                    image.invalidate()
                }
            }
        } catch (_: Exception) {
            gameLoopActive = false
            return
        }
        frameHandler.postDelayed({ renderGameFrame() }, 16L)
    }

    private fun showNativeVideoTest() {
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
            setPadding(12, 8, 12, 8)
            setBackgroundColor(0xFF10151E.toInt())
        }

        val title = TextView(this).apply {
            text = "PRUEBA DE VÍDEO NATIVO · 240 × 160"
            textSize = 16f
            gravity = Gravity.CENTER
            setTextColor(0xFFFFD98A.toInt())
        }

        val image = ImageView(this).apply {
            setBackgroundColor(0xFF05070B.toInt())
            scaleType = ImageView.ScaleType.FIT_CENTER
            contentDescription = "Fotograma gráfico de prueba generado por C++"
        }

        val status = TextView(this).apply {
            textSize = 12f
            gravity = Gravity.CENTER
            setTextColor(0xFFF1F3F8.toInt())
        }

        val backButton = Button(this).apply {
            text = "Volver al selector"
            setOnClickListener { showLauncher() }
        }

        try {
            val pixels = nativeCreateTestFrame()
            if (pixels.size == TEST_FRAME_WIDTH * TEST_FRAME_HEIGHT) {
                image.setImageBitmap(
                    Bitmap.createBitmap(
                        pixels, TEST_FRAME_WIDTH, TEST_FRAME_HEIGHT, Bitmap.Config.ARGB_8888
                    )
                )
                status.text = "Fotograma recibido desde C++ y dibujado en Android.\nEsto valida el transporte de imagen; no es el juego."
            } else {
                status.text = "La prueba devolvió un tamaño inesperado (${pixels.size})."
            }
        } catch (error: Exception) {
            status.text = "No se pudo crear el fotograma: ${error.javaClass.simpleName}"
        }

        root.addView(title, layoutParams(bottom = 4))
        root.addView(image, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f))
        root.addView(status, layoutParams(bottom = 2))
        root.addView(backButton, layoutParams())
        setContentView(root)
    }

    private fun layoutParams(bottom: Int = 0) =
        LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT
        ).apply { bottomMargin = bottom }

    private fun openRomPicker() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "*/*"
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION)
        }
        startActivityForResult(intent, REQUEST_OPEN_ROM)
    }

    @Deprecated("Uses startActivityForResult for compatibility with the current minimal project.")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode != REQUEST_OPEN_ROM || resultCode != RESULT_OK) return

        val uri: Uri = data?.data ?: return
        val displayName = queryDisplayName(uri) ?: uri.lastPathSegment ?: "archivo seleccionado"

        if (!displayName.lowercase(Locale.ROOT).endsWith(".gba")) {
            romStatus.text = "Archivo seleccionado: $displayName\nSelecciona un archivo .gba."
            return
        }

        val header = try {
            readGbaHeader(uri)
        } catch (_: Exception) {
            romStatus.text = "No se pudo leer el archivo. Comprueba los permisos y vuelve a intentarlo."
            return
        }

        if (header == null) {
            romStatus.text = "El archivo es demasiado pequeño o no se pudo leer como una ROM GBA."
            return
        }

        try {
            val grantedFlags = ((data?.flags) ?: 0) and Intent.FLAG_GRANT_READ_URI_PERMISSION
            if (grantedFlags != 0) contentResolver.takePersistableUriPermission(uri, grantedFlags)
        } catch (_: SecurityException) {
            // Some providers do not support persisted grants.
        } catch (_: IllegalArgumentException) {
            // The provider did not offer a persistable permission.
        }

        getSharedPreferences(PREFS, MODE_PRIVATE).edit()
            .putString(KEY_ROM_URI, uri.toString())
            .putString(KEY_ROM_NAME, displayName)
            .apply()

        val sizeText = if (header.sizeBytes > 0) formatSize(header.sizeBytes) else "tamaño desconocido"
        val identity = if (header.looksLikeFireEmblem) {
            "La cabecera parece corresponder a Fire Emblem."
        } else {
            "No se pudo confirmar que sea Fire Emblem; se guardó de todos modos."
        }
        romStatus.text = "Archivo: ${displayName}\nTítulo: ${header.title.ifBlank { "(vacío)" }}\nCódigo: ${header.gameCode.ifBlank { "(vacío)" }} · $sizeText\n$identity\nROM seleccionada y lista para iniciar con mGBA."
    }

    private data class GbaHeader(
        val title: String,
        val gameCode: String,
        val sizeBytes: Long,
        val looksLikeFireEmblem: Boolean
    )

    @Throws(IOException::class)
    private fun readGbaHeader(uri: Uri): GbaHeader? {
        val bytes = ByteArray(GBA_HEADER_SIZE)
        val input = contentResolver.openInputStream(uri) ?: return null
        val readCount = input.use { stream ->
            var total = 0
            while (total < bytes.size) {
                val count = stream.read(bytes, total, bytes.size - total)
                if (count <= 0) break
                total += count
            }
            total
        }
        if (readCount < MIN_GBA_SIZE) return null

        // Native C++ parses the header from bytes read locally on-device.
        val fields = nativeInspectGbaHeader(bytes).split('\n', limit = 3)
        if (fields.size != 3) return null
        return GbaHeader(
            title = fields[0],
            gameCode = fields[1],
            sizeBytes = queryFileSize(uri),
            looksLikeFireEmblem = fields[2] == "1"
        )
    }

    private fun queryFileSize(uri: Uri): Long {
        return try {
            contentResolver.query(uri, arrayOf(OpenableColumns.SIZE), null, null, null)?.use { cursor ->
                if (cursor.moveToFirst()) {
                    val index = cursor.getColumnIndex(OpenableColumns.SIZE)
                    if (index >= 0 && !cursor.isNull(index)) cursor.getLong(index) else -1L
                } else -1L
            } ?: -1L
        } catch (_: Exception) {
            -1L
        }
    }

    private fun formatSize(bytes: Long): String =
        String.format(Locale.ROOT, "%.2f MiB", bytes / (1024.0 * 1024.0))

    private fun restoreSavedRom() {
        val prefs = getSharedPreferences(PREFS, MODE_PRIVATE)
        val uriText = prefs.getString(KEY_ROM_URI, null)
        val name = prefs.getString(KEY_ROM_NAME, null)
        romStatus.text = if (uriText != null && name != null) {
            try {
                val header = readGbaHeader(Uri.parse(uriText))
                if (header != null) {
                    "ROM guardada: $name\nTítulo: ${header.title.ifBlank { "(vacío)" }} · Código: ${header.gameCode.ifBlank { "(vacío)" }}\nPulsa «Iniciar FE7 (motor mGBA)» para jugar."
                } else {
                    "El archivo guardado ya no parece legible. Selecciónalo otra vez."
                }
            } catch (_: Exception) {
                "La ROM guardada ya no está disponible. Selecciónala otra vez."
            }
        } else {
            "Todavía no has seleccionado una ROM."
        }
    }

    private fun queryDisplayName(uri: Uri): String? {
        contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { cursor ->
            if (cursor.moveToFirst()) {
                val index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                if (index >= 0) return cursor.getString(index)
            }
        }
        return null
    }
}
