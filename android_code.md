package com.example.myapplication

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import com.example.myapplication.ui.theme.MyApplicationTheme

class MainActivity : AppCompatActivity() {

    private val REQUEST_ENABLE_BT = 1
    private val REQUEST_FINE_LOCATION = 2
    private val TAG = "MainActivity"
    private lateinit var textView: TextView
    private var bluetoothSocket: BluetoothSocket? = null
    private var inputStream: InputStream? = null
    private val handler = Handler(Looper.getMainLooper())
    private var bluetoothAdapter: BluetoothAdapter? = null
    private lateinit var mainViewModel: MainViewModel


    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        textView = findViewById(R.id.textView)
        mainViewModel = ViewModelProvider(this)[MainViewModel::class.java]

        // Check Bluetooth support
        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = bluetoothManager.adapter
        if (bluetoothAdapter == null) {
            textView.text = "Bluetooth is not supported on this device."
            return
        }

        // Check Bluetooth permissions
        checkBluetoothPermissions()
        observeViewModel()
    }

    private fun observeViewModel(){
        mainViewModel.bluetoothState.observe(this){state ->
            textView.text = state
        }
    }

    private fun checkBluetoothPermissions() {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, arrayOf(Manifest.permission.ACCESS_FINE_LOCATION), REQUEST_FINE_LOCATION)
        } else {
            connectToDevice()
        }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if(requestCode == REQUEST_FINE_LOCATION){
            if(grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED){
                connectToDevice()
            } else {
                textView.text = "Location permission denied. Cannot connect to Bluetooth."
            }
        }
    }

    private fun connectToDevice() {
        if (!bluetoothAdapter!!.isEnabled) {
            val enableBtIntent = Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)
            startActivityForResult(enableBtIntent, REQUEST_ENABLE_BT)
        } else {

            val hc05Address = "XX:XX:XX:XX:XX:XX" //

            val device = bluetoothAdapter?.getRemoteDevice(hc05Address)
            if (device != null){
                mainViewModel.connect(device)
            } else {
                textView.text = "Device not found"
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        try {
            bluetoothSocket?.close()
        } catch (e: IOException) {
            // Ignore errors on socket close
        }
    }
}

class MainViewModel: androidx.lifecycle.ViewModel() {
    private val _bluetoothState = androidx.lifecycle.MutableLiveData<String>("Connecting...")
    val bluetoothState: androidx.lifecycle.LiveData<String> = _bluetoothState

    private var bluetoothSocket: BluetoothSocket? = null
    private var inputStream: InputStream? = null
    private val handler = Handler(Looper.getMainLooper())
    fun connect(device: BluetoothDevice) {
        Thread {
            try {
                bluetoothSocket =
                    device.createRfcommSocketToServiceRecord(UUID.fromString("00001101-0000-1000-8000-00805F9B34FB"))
                bluetoothSocket?.connect()
                inputStream = bluetoothSocket?.inputStream
                startListening()
            } catch (e: IOException) {
                _bluetoothState.postValue("Connection failed: ${e.message}")
            }
        }.start()
    }

    private fun startListening() {
        Thread {
            while (true) {
                try {
                    val buffer = ByteArray(1024)
                    val bytesRead = inputStream?.read(buffer) ?: 0
                    val data = String(buffer, 0, bytesRead)
                    handler.post { _bluetoothState.postValue(data) }
                } catch (e: IOException) {
                    handler.post { _bluetoothState.postValue("Error reading data: ${e.message}") }
                    break
                }
            }
        }.start()
    }

    override fun onCleared() {
        super.onCleared()
        try {
            bluetoothSocket?.close()
        } catch (e: IOException) {
            // Ignore errors on socket close
        }
    }
}
