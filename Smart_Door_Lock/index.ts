import mqtt from "mqtt";
import axios from "axios";

import config from "./edge/config.ts";
import rules from "./edge/rules.ts";

import isInternetAvailable from "./edge/internet.ts";

import storage from "./edge/storage.ts";

const client = mqtt.connect(config.mqttBroker);

client.on("connect", () => {

    console.log("====================================");
    console.log("Edge Computing Berhasil Terhubung");
    console.log("MQTT Broker :", config.mqttBroker);
    console.log("====================================");

    client.subscribe(config.topics.access);

    console.log("Subscribe :", config.topics.access);

});

client.on("message", async (topic, message) => {

    try {

        const data = JSON.parse(message.toString());

        console.log("\n========== DATA RFID ==========");
        console.log(data);

        // ==========================
        // EDGE ANALYSIS
        // ==========================

        const action = rules(data);

        console.log("Hasil Analisis :", action);

        const url =
            `${config.thingsboard.url}${config.thingsboard.token}/telemetry`;

        // ==========================
        // CEK KONEKSI INTERNET
        // ==========================

        const internet = await isInternetAvailable();

        if (internet) {

            console.log("Internet tersedia");

            // ==========================
            // Kirim data offline jika ada
            // ==========================

            const offlineLogs = storage.loadOffline();

            if (offlineLogs.length > 0) {

                console.log(`Mengirim ${offlineLogs.length} data offline...`);

                for (const log of offlineLogs) {

                    await axios.post(url, log, {
                        headers: {
                            "Content-Type": "application/json"
                        }
                    });

                }

                storage.clearOffline();

                console.log("Semua data offline berhasil dikirim");

            }

            // ==========================
            // Kirim data terbaru
            // ==========================

            if (action.sendToCloud) {

                await axios.post(
                    url,
                    {
                        security: data.security,
                        status: data.status,
                        pintu: data.pintu,
                        uid: data.uid,
                        timestamp: data.timestamp,
                        notification: action.notification
                    },
                    {
                        headers: {
                            "Content-Type": "application/json"
                        }
                    }
                );

                console.log("Data terbaru berhasil dikirim");

            }

        } else {

            console.log("Internet tidak tersedia");

            if (action.saveOffline) {

                storage.saveOffline({

                    security: data.security,
                    status: data.status,
                    pintu: data.pintu,
                    uid: data.uid,
                    timestamp: data.timestamp,
                    notification: action.notification

                });

                console.log("Data disimpan ke offline.json");

            }

        }

    } catch (error) {

        console.error("Error :", error);

    }

});