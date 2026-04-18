#ifndef SIMULATIONRESPONSE_H
#define SIMULATIONRESPONSE_H

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaType>
#include <QString>
#include <QStringList>

struct SimulationResponse {
    bool success = false; // true if err is null
    QJsonValue err;       // null or error object
    QStringList logs;
    quint64 unitsConsumed = 0;

    static SimulationResponse fromJson(const QJsonObject& json) {
        // json is the "value" object inside the RPC result
        SimulationResponse sim;
        sim.err = json["err"];
        sim.success = json["err"].isNull();
        sim.unitsConsumed = json["unitsConsumed"].toInteger();

        for (const auto& v : json["logs"].toArray())
            sim.logs.append(v.toString());

        return sim;
    }
};

Q_DECLARE_METATYPE(SimulationResponse)

#endif // SIMULATIONRESPONSE_H
