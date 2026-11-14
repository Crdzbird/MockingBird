package io.mockingbird

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.media.AudioAttributes
import android.media.AudioFocusRequest
import android.media.AudioManager
import android.os.Build
import io.flutter.plugin.common.EventChannel

/**
 * Holds playback focus for one Flutter engine on behalf of every player that
 * runs in it, and reports what happens to it. Focus is requested when the
 * first player starts and abandoned when the last one stops; a player that
 * starts while focus is lost, delayed or ducked is told so at once.
 */
class MockingbirdAudioFocus(private val context: Context) : AudioManager.OnAudioFocusChangeListener {
    private val audio = context.getSystemService(Context.AUDIO_SERVICE) as AudioManager
    private var sink: EventChannel.EventSink? = null
    private var request: AudioFocusRequest? = null
    private var holders = 0
    private var state = GAINED
    private var registered = false
    private val noisy = object : BroadcastReceiver() {
        override fun onReceive(c: Context?, intent: Intent?) {
            if (intent?.action == AudioManager.ACTION_AUDIO_BECOMING_NOISY) send("becomingNoisy")
        }
    }

    /** Where focus changes go; null drops them. */
    fun listen(events: EventChannel.EventSink?) {
        sink = events
    }

    /**
     * Requests focus for spoken media for one more player. A delayed grant
     * reports a transient loss until the grant arrives, a refusal reports a
     * loss, and false means the system refused.
     */
    fun request(): Boolean {
        holders += 1
        if (holders == 1) registerNoisy()
        if (holders > 1 && state != LOST) {
            if (state != GAINED) send(state)
            return true
        }
        val result = if (Build.VERSION.SDK_INT >= 26) requestModern() else requestLegacy()
        state = when (result) {
            AudioManager.AUDIOFOCUS_REQUEST_GRANTED -> GAINED
            AudioManager.AUDIOFOCUS_REQUEST_DELAYED -> LOST_TRANSIENT
            else -> LOST
        }
        if (state != GAINED) send(state)
        return result != AudioManager.AUDIOFOCUS_REQUEST_FAILED
    }

    /** One player stopped; the last one gives focus back and stops listening for the noisy broadcast. */
    fun abandon() {
        if (holders == 0) return
        holders -= 1
        if (holders == 0) release()
    }

    /** Gives focus back whatever the number of players, as the engine goes away. */
    fun release() {
        holders = 0
        unregisterNoisy()
        if (Build.VERSION.SDK_INT >= 26) {
            request?.let { audio.abandonAudioFocusRequest(it) }
        } else {
            @Suppress("DEPRECATION")
            audio.abandonAudioFocus(this)
        }
        request = null
        state = GAINED
    }

    override fun onAudioFocusChange(change: Int) {
        state = when (change) {
            AudioManager.AUDIOFOCUS_GAIN -> GAINED
            AudioManager.AUDIOFOCUS_LOSS -> LOST
            AudioManager.AUDIOFOCUS_LOSS_TRANSIENT -> LOST_TRANSIENT
            AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK -> DUCK
            else -> return
        }
        send(state)
    }

    private fun registerNoisy() {
        if (registered) return
        registered = true
        val filter = IntentFilter(AudioManager.ACTION_AUDIO_BECOMING_NOISY)
        if (Build.VERSION.SDK_INT >= 33) {
            context.registerReceiver(noisy, filter, Context.RECEIVER_NOT_EXPORTED)
        } else {
            context.registerReceiver(noisy, filter)
        }
    }

    private fun unregisterNoisy() {
        if (!registered) return
        registered = false
        context.unregisterReceiver(noisy)
    }

    private fun requestModern(): Int {
        val attributes = AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_MEDIA)
            .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
            .build()
        val r = AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
            .setAudioAttributes(attributes)
            .setAcceptsDelayedFocusGain(true)
            .setOnAudioFocusChangeListener(this)
            .build()
        request = r
        return audio.requestAudioFocus(r)
    }

    @Suppress("DEPRECATION")
    private fun requestLegacy(): Int =
        audio.requestAudioFocus(this, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN)

    private fun send(name: String) {
        sink?.success(name)
    }

    private companion object {
        const val GAINED = "gained"
        const val LOST = "lost"
        const val LOST_TRANSIENT = "lostTransient"
        const val DUCK = "duck"
    }
}
