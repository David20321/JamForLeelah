using UnityEngine;
using System.Collections;

public class staticVariables : MonoBehaviour {

	public static float characterSeed;
	public static float lightSeed;

	public float showCharacterSeed = 10;
	public float showLightSeed = 10;

	void Awake () {

		characterSeed = showCharacterSeed;
		lightSeed = showLightSeed;

	}

}
