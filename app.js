
import { initializeApp } from "https://www.gstatic.com/firebasejs/10.12.2/firebase-app.js";

import {
    getDatabase,
    ref,
    onValue
} from "https://www.gstatic.com/firebasejs/10.12.2/firebase-database.js";

// =====================================================
// ARNOLD FIREBASE CONFIGURATION
// =====================================================

const firebaseConfig = {
    apiKey: "AIzaSyB9-FYcrAgyAPitLZrJpxyMZs27TEtc9AQ",
    authDomain: "arnold-28517.firebaseapp.com",
    databaseURL: "https://arnold-28517-default-rtdb.europe-west1.firebasedatabase.app/",
    projectId: "arnold-28517",
    storageBucket: "arnold-28517.firebasestorage.app",
    messagingSenderId: "443115176489",
    appId: "1:443115176489:web:844e596c22d9e63acb5238"
    
};

// =====================================================
// INITIALIZE FIREBASE
// =====================================================

const app = initializeApp(firebaseConfig);
const database = getDatabase(app);

const sensorRef = ref(database, "ESP32_Data");

// =====================================================
// GLOBAL VARIABLES
// =====================================================

let allSensorRecords = [];
let sensorChart = null;

let relay1State = false;
let relay2State = false;

// =====================================================
// MANILA DATE AND TIME
// =====================================================

function getManilaDate() {
    return new Intl.DateTimeFormat("en-CA", {
        timeZone: "Asia/Manila",
        year: "numeric",
        month: "2-digit",
        day: "2-digit"
    }).format(new Date());
}

function updateClock() {
    const now = new Date();

    const time = new Intl.DateTimeFormat("en-PH", {
        timeZone: "Asia/Manila",
        hour: "2-digit",
        minute: "2-digit",
        second: "2-digit",
        hour12: true
    }).format(now);

    const date = new Intl.DateTimeFormat("en-PH", {
        timeZone: "Asia/Manila",
        weekday: "long",
        year: "numeric",
        month: "long",
        day: "numeric"
    }).format(now);

    document.getElementById("liveClock").textContent = time;
    document.getElementById("liveDate").textContent = date;
}

updateClock();
setInterval(updateClock, 1000);

// =====================================================
// CONNECTION STATUS
// =====================================================

function setFirebaseStatus(connected, message) {
    const status = document.getElementById("firebaseStatus");
    const dot = document.getElementById("firebaseDot");

    status.textContent = message;

    dot.classList.toggle("online", connected);
    dot.classList.toggle("offline", !connected);
}

function setESPStatus(connected) {
    const status = document.getElementById("connectionText");

    status.textContent = connected ? "CONNECTED" : "DISCONNECTED";

    const dot = document.querySelector(".system-indicator .status-dot");

    dot.classList.toggle("online", connected);
    dot.classList.toggle("offline", !connected);
}

// =====================================================
// FIREBASE DATA LISTENER
// =====================================================

onValue(
    sensorRef,

    (snapshot) => {

        const data = snapshot.val();

        if (!data) {
            allSensorRecords = [];

            setFirebaseStatus(true, "CONNECTED - NO DATA");

            renderDashboard();
            return;
        }

        allSensorRecords = [];

        Object.keys(data).forEach((dateKey) => {

            const dailyRecords = data[dateKey];

            if (!dailyRecords || typeof dailyRecords !== "object") {
                return;
            }

            Object.keys(dailyRecords).forEach((timeKey) => {

                const record = dailyRecords[timeKey];

                if (!record || typeof record !== "object") {
                    return;
                }

                const temperature = Number(record.temperature);
                const humidity = Number(record.humidity);

                if (
                    !Number.isFinite(temperature) ||
                    !Number.isFinite(humidity)
                ) {
                    return;
                }

                allSensorRecords.push({
                    date: dateKey,
                    time: timeKey,
                    temperature: temperature,
                    humidity: humidity
                });

            });

        });

        allSensorRecords.sort((a, b) => {
            const dateA = a.date + " " + a.time;
            const dateB = b.date + " " + b.time;

            return dateA.localeCompare(dateB);
        });

        setFirebaseStatus(true, "CONNECTED");
        setESPStatus(true);

        renderDashboard();
    },

    (error) => {

        console.error("Firebase error:", error);

        setFirebaseStatus(false, "CONNECTION ERROR");
    }
);

// =====================================================
// DASHBOARD RENDERING
// =====================================================

function renderDashboard() {

    const selectedDate =
        document.getElementById("dateFilter").value || getManilaDate();

    const selectedRecords = allSensorRecords.filter(record => {
        return record.date === selectedDate;
    });

    renderCurrentReadings();
    renderChart(selectedRecords);
    renderHistory(selectedRecords);

    document.getElementById("recordCount").textContent =
        selectedRecords.length;
}

// =====================================================
// CURRENT TEMPERATURE AND HUMIDITY
// =====================================================

function renderCurrentReadings() {

    if (allSensorRecords.length === 0) {
        document.getElementById("temperatureValue").textContent = "--.-";
        document.getElementById("humidityValue").textContent = "--.-";

        document.getElementById("temperatureStatus").textContent =
            "AWAITING SENSOR";

        document.getElementById("humidityStatus").textContent =
            "AWAITING SENSOR";

        document.getElementById("lastUpdate").textContent =
            "No sensor records";

        return;
    }

    const latest = allSensorRecords[allSensorRecords.length - 1];

    document.getElementById("temperatureValue").textContent =
        latest.temperature.toFixed(1);

    document.getElementById("humidityValue").textContent =
        latest.humidity.toFixed(1);

    document.getElementById("temperatureStatus").textContent =
        "SENSOR ACTIVE";

    document.getElementById("humidityStatus").textContent =
        "SENSOR ACTIVE";

    document.getElementById("lastUpdate").textContent =
        latest.date + " / " + latest.time;
}

// =====================================================
// CHART INITIALIZATION
// =====================================================

function initializeChart() {

    const canvas = document.getElementById("sensorChart");

    if (!canvas || typeof Chart === "undefined") {
        console.error("Chart.js is not loaded.");
        return;
    }

    sensorChart = new Chart(canvas, {

        type: "line",

        data: {
            labels: [],

            datasets: [

                {
                    label: "Temperature (°C)",
                    data: [],
                    borderColor: "#22d3ee",
                    backgroundColor: "rgba(34, 211, 238, 0.10)",
                    borderWidth: 2,
                    pointRadius: 2,
                    pointHoverRadius: 5,
                    tension: 0.35,
                    fill: true,
                    yAxisID: "y"
                },

                {
                    label: "Humidity (%)",
                    data: [],
                    borderColor: "#38bdf8",
                    backgroundColor: "rgba(56, 189, 248, 0.08)",
                    borderWidth: 2,
                    pointRadius: 2,
                    pointHoverRadius: 5,
                    tension: 0.35,
                    fill: false,
                    yAxisID: "y1"
                }

            ]
        },

        options: {

            responsive: true,
            maintainAspectRatio: false,

            interaction: {
                mode: "index",
                intersect: false
            },

            plugins: {

                legend: {
                    display: false
                },

                tooltip: {
                    backgroundColor: "#0c131d",
                    titleColor: "#22d3ee",
                    bodyColor: "#e2e8f0",
                    borderColor: "#263449",
                    borderWidth: 1
                }

            },

            scales: {

                x: {
                    grid: {
                        color: "rgba(148, 163, 184, 0.08)"
                    },

                    ticks: {
                        color: "#64748b",
                        maxTicksLimit: 10,
                        font: {
                            size: 9
                        }
                    }
                },

                y: {
                    type: "linear",
                    position: "left",

                    title: {
                        display: true,
                        text: "Temperature (°C)",
                        color: "#22d3ee"
                    },

                    grid: {
                        color: "rgba(148, 163, 184, 0.08)"
                    },

                    ticks: {
                        color: "#22d3ee",
                        font: {
                            size: 9
                        }
                    }
                },

                y1: {
                    type: "linear",
                    position: "right",

                    title: {
                        display: true,
                        text: "Humidity (%)",
                        color: "#38bdf8"
                    },

                    min: 0,
                    max: 100,

                    grid: {
                        drawOnChartArea: false
                    },

                    ticks: {
                        color: "#38bdf8",
                        font: {
                            size: 9
                        }
                    }
                }

            }

        }

    });

}

// =====================================================
// UPDATE CHART
// =====================================================

function renderChart(records) {

    if (!sensorChart) return;

    sensorChart.data.labels = records.map(record => record.time);

    sensorChart.data.datasets[0].data =
        records.map(record => record.temperature);

    sensorChart.data.datasets[1].data =
        records.map(record => record.humidity);

    sensorChart.update();
}

// =====================================================
// HISTORY TABLE
// =====================================================

function renderHistory(records) {

    const historyBody = document.getElementById("historyBody");

    historyBody.innerHTML = "";

    if (records.length === 0) {

        historyBody.innerHTML = `
            <tr>
                <td colspan="6" class="empty-state">
                    No records found for the selected date.
                </td>
            </tr>
        `;

        return;
    }

    const reversedRecords = [...records].reverse();

    reversedRecords.forEach((record, index) => {

        const row = document.createElement("tr");

        const number = index + 1;

        row.innerHTML = `
            <td>${number}</td>
            <td>${record.date}</td>
            <td>${record.time}</td>
            <td>${record.temperature.toFixed(1)} °C</td>
            <td>${record.humidity.toFixed(1)} %</td>
            <td><span class="normal-status">● RECORDED</span></td>
        `;

        historyBody.appendChild(row);

    });

}

// =====================================================
// DATE FILTER
// =====================================================

document.getElementById("dateFilter").value = getManilaDate();

document.getElementById("dateFilter").addEventListener(
    "change",
    renderDashboard
);

// =====================================================
// RELAY STATUS DISPLAY
// =====================================================

function updateRelayDisplay(relayNumber, state) {

    const status = document.getElementById(
        `relay${relayNumber}Status`
    );

    const button = document.getElementById(
        `relay${relayNumber}Button`
    );

    const dot = document.getElementById(
        `relay${relayNumber}Dot`
    );

    if (!status || !button || !dot) return;

    status.textContent = state ? "ON" : "OFF";

    button.textContent = state ? "TURN OFF" : "TURN ON";

    button.classList.toggle("is-on", state);

    dot.classList.toggle("online", state);
    dot.classList.toggle("offline", !state);

    if (relayNumber === 1) {
        relay1State = state;
    } else {
        relay2State = state;
    }
}

// =====================================================
// GET RELAY STATUS FROM ESP32
// =====================================================

async function fetchRelayStatus() {

    try {

        const response = await fetch("/relay-status", {
            cache: "no-store"
        });

        if (!response.ok) {
            throw new Error("Relay status request failed.");
        }

        const data = await response.json();

        updateRelayDisplay(1, data.LED1 === true);
        updateRelayDisplay(2, data.LED2 === true);

    } catch (error) {

        console.error("Relay status error:", error);

    }

}

// =====================================================
// CONTROL RELAY
// =====================================================

window.toggleRelay = async function(relayNumber) {

    const currentState =
        relayNumber === 1 ? relay1State : relay2State;

    const newState = currentState ? "off" : "on";

    const button = document.getElementById(
        `relay${relayNumber}Button`
    );

    button.disabled = true;

    try {

        const response = await fetch(
            `/relay?relay=${relayNumber}&state=${newState}`,
            {
                method: "GET",
                cache: "no-store"
            }
        );

        if (!response.ok) {
            throw new Error("Relay command failed.");
        }

        const result = await response.json();

        if (!result.success) {
            throw new Error("ESP32 rejected relay command.");
        }

        updateRelayDisplay(relayNumber, newState === "on");

    } catch (error) {

        console.error("Relay control error:", error);

        alert(
            "Unable to control relay. Please check the ESP32 connection."
        );

    } finally {

        button.disabled = false;

    }

};

// =====================================================
// INITIALIZE DASHBOARD
// =====================================================

initializeChart();

fetchRelayStatus();

setInterval(fetchRelayStatus, 3000);

setESPStatus(true);
