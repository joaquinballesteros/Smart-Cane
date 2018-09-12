package com.example.juan.ble_cane;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.Environment;
import android.support.annotation.NonNull;
import android.support.design.widget.TextInputEditText;
import android.support.design.widget.TextInputLayout;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.text.TextUtils;
import android.util.Log;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.RadioButton;
import android.widget.Toast;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.text.SimpleDateFormat;
import java.util.Date;

public class IdActivity extends AppCompatActivity {

    private TextInputEditText editTextId;
    private TextInputEditText editTextAge;
    private TextInputLayout textInputId;
    private TextInputLayout textInputAge;
    private RadioButton radiobtnMale, radiobtnFemale,radiobtnRight, radiobtnLeft;
    private Button btnSave, btnRaw, btnAct;

    SimpleDateFormat dateStr = new SimpleDateFormat("dd-MM-yyyy HH:mm:ss");
    String fullPath = Environment.getExternalStorageDirectory().getAbsolutePath() + "/Cane";
    File dir;
    File subdir;
    OutputStream fOut;
    File file;
    String fDate;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_id);

        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);

        editTextId = (TextInputEditText) findViewById(R.id.editTextId);
        editTextAge = (TextInputEditText) findViewById(R.id.editTextAge);
        textInputId = (TextInputLayout) findViewById(R.id.text_input_layout_id);
        textInputAge = (TextInputLayout) findViewById(R.id.text_input_layout_age);
        radiobtnMale = (RadioButton) findViewById(R.id.male_radio_btn);
        radiobtnFemale = (RadioButton) findViewById(R.id.female_radio_btn);
        radiobtnRight = (RadioButton) findViewById(R.id.right_radio_btn);
        radiobtnLeft = (RadioButton) findViewById(R.id.left_radio_btn);
        btnSave = (Button) findViewById(R.id.buttonValidate);
        btnRaw = (Button) findViewById(R.id.buttonRaw);
        btnAct = (Button) findViewById(R.id.buttonActivity);

        btnRaw.setEnabled(false);
        btnAct.setEnabled(false);

        try {
            String nameSubdir;
            int max = 0;
            int temp;
            dir = new File(fullPath);
            if (!dir.exists()) {
                dir.mkdirs();
            } else {
                File f = new File(fullPath);
                File[] files = f.listFiles();
                for(int i=0; i<files.length; i++) {
                    nameSubdir = files[i].toString();
                    nameSubdir = nameSubdir.replace(fullPath + "/@", "");
                    temp = Integer.valueOf(nameSubdir);
                    if (temp > max){
                        max = temp;
                    }
                }
                int index = Integer.valueOf(max) + 1;
                editTextId.setText(String.valueOf(index));
            }
        } catch(Exception e) {
            e.printStackTrace();
        }

        btnSave.setOnClickListener(new View.OnClickListener(){
            @Override
            public void onClick(View view){
                String strGender, strLaterality, strPaciente;
                if(validate()){
                    if(radiobtnFemale.isChecked()){
                        strGender = "Female";
                    } else {
                        strGender = "Male";
                    }

                    if(radiobtnRight.isChecked()){
                        strLaterality = "Right";
                    } else {
                        strLaterality = "Left";
                    }
                    try {
                        subdir = new File(fullPath + "/@" +  editTextId.getText());
                        if (!subdir.exists()) {
                            subdir.mkdirs();
                        }
                        fOut = null;
                        Date fileDate = new Date();
                        fDate = dateStr.format(fileDate)+".txt";
                        file = new File(fullPath + "/@" +  editTextId.getText(), fDate);
                        file.createNewFile();
                        fOut = new FileOutputStream(file, true);
                        strPaciente = "ID: " + editTextId.getText() + "\n" + "Age: " + editTextAge.getText() + "\n" + "Gender: " + strGender
                                + "\n" + "Laterality: " + strLaterality + "\n\n";
                        fOut.write(strPaciente.getBytes());
                    } catch (Exception e){
                        Log.e ("saveToExternalStorage()", e.getMessage());
                    }

                    try {
                        fOut.flush();
                        fOut.close();
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                    Toast.makeText(getApplicationContext(), "Pacient data saved", Toast.LENGTH_SHORT).show();
                    btnSave.setEnabled(false);
                    btnRaw.setEnabled(true);
                    btnAct.setEnabled(true);
                }
            }
        });

        btnAct.setOnClickListener(new View.OnClickListener(){
            @Override
            public void onClick(View view){
                launchActivity();
            }
        });

        btnRaw.setOnClickListener(new View.OnClickListener(){
            @Override
            public void onClick(View view){
                launchRaw();
            }
        });
    }

    private void launchActivity(){
        Intent intentAct = new Intent(this, BLEDataActivity.class);
        intentAct.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        intentAct.putExtra("textName", fDate);
        intentAct.putExtra("textId", "/@" + editTextId.getText());
        startActivity(intentAct);
        finish();
    }

    private void launchRaw(){
        Intent intentRaw = new Intent(this, RawActivity.class);
        intentRaw.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        intentRaw.putExtra("textName", fDate);
        intentRaw.putExtra("textId", "/@" + editTextId.getText());
        startActivity(intentRaw);
        finish();
    }

    private boolean validate() {
        String idError = null;
        boolean validId, validAge, validGender, validLat;
        if (TextUtils.isEmpty(editTextId.getText())) {
            idError = getString(R.string.mandatory);
            validId = false;
        } else {
            validId = true;
        }
        toggleTextInputLayoutError(textInputId, idError);

        String ageError = null;
        if (TextUtils.isEmpty(editTextAge.getText())) {
            ageError = getString(R.string.mandatory);
            validAge = false;
        } else {
            validAge = true;
        }
        toggleTextInputLayoutError(textInputAge, ageError);

        if(!radiobtnFemale.isChecked() && !radiobtnMale.isChecked()){
            radiobtnFemale.setError("Selec Item");
            validGender = false;
        } else {
            radiobtnFemale.setError(null);
            validGender = true;
        }

        if(!radiobtnRight.isChecked() && !radiobtnLeft.isChecked()){
            radiobtnLeft.setError("Selec Item");
            validLat = false;
        } else {
            radiobtnLeft.setError(null);
            validLat = true;
        }

        clearFocus();
        if(!validAge || !validGender || !validId || !validLat){
            return false;
        } else{
            return true;
        }
    }

    /**
     * Display/hides TextInputLayout error.
     *
     * @param msg the message, or null to hide
     */
    private static void toggleTextInputLayoutError(@NonNull TextInputLayout textInputLayout,
                                                   String msg) {
        textInputLayout.setError(msg);
        if (msg == null) {
            textInputLayout.setErrorEnabled(false);
        } else {
            textInputLayout.setErrorEnabled(true);
        }
    }

    private void clearFocus() {
        View view = this.getCurrentFocus();
        if (view != null && view instanceof EditText) {
            InputMethodManager inputMethodManager = (InputMethodManager) getSystemService(Context
                    .INPUT_METHOD_SERVICE);
            inputMethodManager.hideSoftInputFromWindow(view.getWindowToken(), 0);
            view.clearFocus();
        }
    }
}
