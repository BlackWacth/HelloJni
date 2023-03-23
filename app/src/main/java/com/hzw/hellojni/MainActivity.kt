package com.hzw.hellojni

import android.annotation.SuppressLint
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.util.Log
import android.widget.TextView
import androidx.annotation.Keep
import com.hzw.hellojni.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private var hour = 0
    private var minute = 0
    private var second = 0

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)
    }

    override fun onResume() {
        super.onResume()
        startTicks()
    }

    override fun onStop() {
        stopTicks()
        super.onStop()
    }

    @SuppressLint("SetTextI18n")
    @Keep
    private fun updateTimer() {
        ++second
        if (second >= 60) {
            ++minute
            if (minute >= 60) {
                ++hour
                minute -= 60
            }
        }
        Log.i("hzw", "current Thread => ${Thread.currentThread()}")

        runOnUiThread {
            binding.tickView.text = "${hour}:${minute}:${second}"
        }
    }

    external fun startTicks()

    external fun stopTicks()

    companion object {
        init {
            System.loadLibrary("hellojni")
        }
    }
}