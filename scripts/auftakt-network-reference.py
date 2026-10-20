"""Independent NumPy evaluation of the recovered three-layer bidirectional LSTM."""
import argparse
import base64
import json
from pathlib import Path
import numpy as np


def tensor(value):
    return np.frombuffer(base64.b64decode(''.join(value), validate=True), dtype='<f4').copy()


def predict(model, inputs):
    x = np.asarray(inputs, dtype=np.float32)
    for name in ('bidirectional', 'bidirectional_1', 'bidirectional_2'):
        layer = model['trainable_params'][name]
        outputs = []
        for direction in ('forward', 'backward'):
            w = tensor(layer[direction + '_weights'])
            r = tensor(layer[direction + '_recurrent_weights'])
            w = w.reshape(x.shape[1], 100)
            r = r.reshape(25, 100)
            bias = tensor(layer[direction + '_bias'])
            h, c = np.zeros(25, dtype=np.float32), np.zeros(25, dtype=np.float32)
            y = np.zeros((len(x), 25), dtype=np.float32)
            for frame in (range(len(x)) if direction == 'forward' else range(len(x) - 1, -1, -1)):
                z = x[frame] @ w + h @ r + bias
                i, f, candidate, o = np.split(z, 4)
                sigmoid = lambda v: 1 / (1 + np.exp(-np.clip(v, -80, 80)))
                c = sigmoid(f) * c + sigmoid(i) * np.tanh(candidate)
                h = sigmoid(o) * np.tanh(c)
                y[frame] = h
            outputs.append(y)
        x = np.concatenate(outputs, axis=1)
    layer = model['trainable_params']['time_distributed']
    w = tensor(layer['weights'])
    w = w.reshape(50, 3)
    z = x @ w + tensor(layer['bias'])
    z -= z.max(axis=1, keepdims=True)
    e = np.exp(z)
    return e / e.sum(axis=1, keepdims=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--model', type=Path, required=True)
    args = parser.parse_args()
    model = json.loads(args.model.read_text())
    test = model['tests'][0]
    x = tensor(test['inputs'][0]['values']).reshape(32, 320)
    expected = tensor(test['outputs'][0]['values']).reshape(32, 3)
    actual = predict(model, x)
    error = float(np.max(np.abs(actual - expected)))
    print('Packaged network test maximum absolute error:', error)
    raise SystemExit(0 if error < 1e-5 else 1)
