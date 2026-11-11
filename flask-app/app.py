from flask import Flask, jsonify, render_template

app = Flask(__name__)

DEVICE = "/dev/hcsr04"


def read_distance_mm():
    with open(DEVICE, "r", encoding="utf-8") as f:
        return int(f.read().strip())


@app.get("/api/distance")
def api_distance():
    try:
        mm = read_distance_mm()
        return jsonify({
            "ok": True,
            "distance_mm": mm,
            "distance_cm": round(mm / 10.0, 1)
        })
    except Exception as exc:
        return jsonify({
            "ok": False,
            "error": str(exc)
        }), 503


@app.get("/")
def index():
    return render_template("index.html")


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)
