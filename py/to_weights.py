import torch
import numpy as np

base = './models/session4/'
# file = f"{base}model_120.pt"
file = f"{base}model_120.pt"
model = torch.load(file)

parameters = {}

for name, parameter in model.items():
    name: str
    # layer1.weight, layer1.bias
    i = int(name[len("layer")])
    parameters[i] = parameters.get(i, [None, None])

    if name.endswith("weight"):
        parameters[i][0] = parameter.cpu().numpy()
    elif name.endswith("bias"):
        parameters[i][1] = parameter.cpu().numpy()

    print(f'loaded {name} with {parameter.shape}')

for k, parameter in parameters.items():
    out = []
    outputs, inputs = parameter[0].shape
    out.append(inputs)
    out.append(outputs)

    for i in range(inputs * outputs):
        out.append(np.transpose(parameter[0]).flat[i])

    for i in range(outputs):
        out.append(parameter[1].flat[i])

    out_str = []
    for i in out:
        if isinstance(i, int):
            out_str.append(str(i))
        else:
            out_str.append(f"{i:.15f}")

    print('written to ' + f'{base}weight_{k}.txt')
    with open(f'{base}weight_{k}.txt', 'w') as f:
        f.write(' '.join(out_str))
